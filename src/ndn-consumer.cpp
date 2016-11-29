#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

#include <ndn-cxx/exclude.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/name-component.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/backports.hpp>

#include "logger.hpp"

namespace ndn {

class NdnConsumer : boost::noncopyable
{
public:
  explicit
  NdnConsumer(const char* programName)
    : m_programName(programName)
    , m_logger("NdnConsumer")
    , m_instanceId(to_string(std::rand()))
    , m_hasError(false)
    , m_hasQuietLogging(false)
    , m_interestInterval(getDefaultInterestInterval())
    , m_nMaximumInterests(-1)
    , m_face(m_ioService)
    , m_nInterestsSent(0)
    , m_nInterestsReceived(0)
    , m_nNacks(0)
    , m_minimumInterestRoundTripTime(std::numeric_limits<double>::max())
    , m_maximumInterestRoundTripTime(0)
    , m_totalInterestRoundTripTime(0)
  {
  }

  bool
  hasError() const
  {
    return m_hasError;
  }

  void
  usage() const
  {
    std::cout << "Usage:\n"
              << "  " << m_programName << " [options] <Traffic_Configuration_File>\n"
              << "\n"
              << "Generate Interest traffic as per provided Traffic Configuration File.\n"
              << "Interests are continuously generated unless a total number is specified.\n"
              << "Set environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.\n"
              << "\n"
              << "Options:\n"
              << "  [-i interval] - set interest generation interval in microseconds (default "
              << getDefaultInterestInterval() << ")\n"
              << "  [-c count]    - set total number of interests to be generated\n"
              << "  [-q]          - quiet mode: no interest reception/data generation logging\n"
              << "  [-h]          - print this help text and exit\n";
    exit(EXIT_FAILURE);
  }

  static time::microseconds
  getDefaultInterestInterval()
  {
    return time::microseconds(1000000);
  }

  void
  setInterestInterval(int interestInterval)
  {
    if (interestInterval <= 0)
      usage();
    m_interestInterval = time::microseconds(interestInterval);
  }

  void
  setMaximumInterests(int maximumInterests)
  {
    if (maximumInterests <= 0)
      usage();
    m_nMaximumInterests = maximumInterests;
  }

  void
  setConfigurationFile(char* configurationFile)
  {
    m_configurationFile = configurationFile;
  }

  void
  setQuietLogging()
  {
    m_hasQuietLogging = true;
  }

  void
  signalHandler()
  {
    logStatistics();

    m_logger.shutdownLogger();
    m_face.shutdown();
    m_ioService.stop();

    exit(m_hasError ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  void
  logStatistics()
  {
    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Interests Sent        = " +
      to_string(m_nInterestsSent), false, true);
    m_logger.log("Total Responses Received    = " +
      to_string(m_nInterestsReceived), false, true);
    m_logger.log("Total Nacks Received        = " +
      to_string(m_nNacks), false, true);

    double loss = 0;
    if (m_nInterestsSent > 0)
      loss = (m_nInterestsSent - m_nInterestsReceived) * 100.0 / m_nInterestsSent;
    m_logger.log("Total Interest Loss         = " + to_string(loss) + "%", false, true);

    double average = 0;
    if (m_nInterestsReceived > 0)
      {
        average = m_totalInterestRoundTripTime / m_nInterestsReceived;
      }
    m_logger.log("Total Round Trip Time       = " +
      to_string(m_totalInterestRoundTripTime) + "ms", false, true);
    m_logger.log("Average Round Trip Time     = " +
      to_string(average) + "ms\n", false, true);
  }

  void
  initializeTrafficConfiguration()
  {
    if (boost::filesystem::exists(boost::filesystem::path(m_configurationFile)))
      {
    	m_traffic_file.open(m_configurationFile.c_str());
      }
    else
      {
        m_logger.log("ERROR - Traffic Configuration File Does Not Exist: " +
                     m_configurationFile, false, true);
        m_logger.shutdownLogger();
        exit(EXIT_FAILURE);
      }
  }

  name::Component
  generateNameComponent()
  {
    std::string str;
    m_traffic_file >> str;
    return name::Component(str);
  }

  void
  onData(const ndn::Interest& interest,
         const ndn::Data& data,
         int globalReference,
         time::steady_clock::TimePoint sentTime)
  {
    std::string logLine = "Data Received      - GlobalID=" + to_string(globalReference);
    logLine += ", Name=" + interest.getName().toUri();

    m_nInterestsReceived++;

    if (!m_hasQuietLogging)
      m_logger.log(logLine, true, false);
    double roundTripTime = (time::steady_clock::now() - sentTime).count() / 1000000.0;
    if (m_minimumInterestRoundTripTime > roundTripTime)
      m_minimumInterestRoundTripTime = roundTripTime;
    if (m_maximumInterestRoundTripTime < roundTripTime)
      m_maximumInterestRoundTripTime = roundTripTime;
    m_totalInterestRoundTripTime += roundTripTime;
    if (m_nMaximumInterests >= 0 && globalReference == m_nMaximumInterests)
      {
        logStatistics();
        m_logger.shutdownLogger();
        m_face.shutdown();
        m_ioService.stop();
      }
  }

  void
  onNack(const ndn::Interest& interest,
         const ndn::lp::Nack& nack,
         int globalReference)
  {
    std::string logLine = "Interest Nack'd    - GlobalID=" + to_string(globalReference);
    logLine += ", Name=" + interest.getName().toUri();
    logLine += ", NackReason=" + to_string((int)nack.getReason());
    m_logger.log(logLine, true, false);

    m_nNacks++;

    if (m_nMaximumInterests >= 0 && globalReference == m_nMaximumInterests) {
      logStatistics();
      m_logger.shutdownLogger();
      m_face.shutdown();
      m_ioService.stop();
    }
  }

  void
  onTimeout(const ndn::Interest& interest,
            int globalReference)
  {
    std::string logLine = "Interest Timed Out - GlobalID=" + to_string(globalReference);
    logLine += ", Name=" + interest.getName().toUri();
    m_logger.log(logLine, true, false);
    if (m_nMaximumInterests >= 0 && globalReference == m_nMaximumInterests)
      {
        logStatistics();
        m_logger.shutdownLogger();
        m_face.shutdown();
        m_ioService.stop();
      }
  }

  void
  generateTraffic(boost::asio::deadline_timer* timer)
  {
    if (m_nMaximumInterests < 0 || m_nInterestsSent < m_nMaximumInterests)
      {
    	Name interestName("/ndn/app");
    	interestName.append(generateNameComponent());
    	Interest interest(interestName);
    	try {
    	  m_nInterestsSent++;
    	  time::steady_clock::TimePoint sentTime = time::steady_clock::now();
          m_face.expressInterest(interest,
                                 bind(&NdnConsumer::onData,
                                      this, _1, _2, m_nInterestsSent, sentTime),
								 bind(&NdnConsumer::onNack,
									  this, _1, _2, m_nInterestsSent),
                                 bind(&NdnConsumer::onTimeout,
                                      this, _1, m_nInterestsSent));

          if (!m_hasQuietLogging) {
            std::string logLine =
              "Sending Interest   - GlobalID=" + to_string(m_nInterestsSent) +
              ", Name=" + interest.getName().toUri();
            m_logger.log(logLine, true, false);
          }
          timer->expires_at(timer->expires_at() +
                            boost::posix_time::microsec(m_interestInterval.count()));
          timer->async_wait(bind(&NdnConsumer::generateTraffic, this, timer));
        }
        catch (const std::exception& e) {
          m_logger.log("ERROR: " + std::string(e.what()), true, true);
        }
     }
  }

  void
  run()
  {
    boost::asio::signal_set signalSet(m_ioService, SIGINT, SIGTERM);
    signalSet.async_wait(bind(&NdnConsumer::signalHandler, this));

    m_instanceId = to_string(1000000/m_interestInterval.count())
    		+ "_" + to_string(m_nMaximumInterests)
			+ "_" + m_configurationFile.substr(0, m_configurationFile.length() - 5)
			+ m_instanceId;
    m_logger.initializeLog(m_instanceId);
    initializeTrafficConfiguration();

    if (m_nMaximumInterests == 0)
      {
        logStatistics();
        m_logger.shutdownLogger();
        return;
      }

    boost::asio::deadline_timer deadlineTimer(m_ioService,
      boost::posix_time::microsec(m_interestInterval.count()));
    deadlineTimer.async_wait(bind(&NdnConsumer::generateTraffic, this, &deadlineTimer));

    try {
      m_face.processEvents();
    }
    catch (const std::exception& e) {
      m_logger.log("ERROR: " + std::string(e.what()), true, true);
      m_logger.shutdownLogger();
      m_hasError = true;
      m_ioService.stop();
    }
  }

private:
  std::string m_programName;
  Logger m_logger;
  std::string m_instanceId;
  bool m_hasError;
  bool m_hasQuietLogging;
  time::microseconds m_interestInterval;
  int m_nMaximumInterests;
  std::string m_configurationFile;
  std::ifstream m_traffic_file;
  boost::asio::io_service m_ioService;
  Face m_face;
  std::vector<uint32_t> m_nonces;
  int m_nInterestsSent;
  int m_nInterestsReceived;
  int m_nNacks;

  double m_minimumInterestRoundTripTime;
  double m_maximumInterestRoundTripTime;
  double m_totalInterestRoundTripTime;
};

} // namespace ndn

int
main(int argc, char* argv[])
{
  std::srand(std::time(nullptr));

  ndn::NdnConsumer client(argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hqi:c:")) != -1) {
    switch (char(option)) {
    case 'h':
      client.usage();
      break;
    case 'i':
      client.setInterestInterval(atoi(optarg));
      break;
    case 'c':
      client.setMaximumInterests(atoi(optarg));
      break;
    case 'q':
      client.setQuietLogging();
      break;
    default:
      client.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (!argc)
    client.usage();

  client.setConfigurationFile(argv[0]);
  client.run();

  return client.hasError() ? EXIT_FAILURE : EXIT_SUCCESS;
}
