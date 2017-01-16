#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/backports.hpp>

#include "logger.hpp"

namespace ndn {

class NdnProducer : boost::noncopyable
{
public:
  explicit
  NdnProducer(const char* programName)
    : m_logger("NdnProducer")
    , m_programName(programName)
    , m_hasError(false)
    , m_hasQuietLogging(false)
    , m_nRegistrationsFailed(0)
    , m_nInterestsSent(0)
    , m_nInterestsReceived(0)
    , m_instanceId(to_string(std::rand()))
    , m_face(m_ioService)
  {
  }

  void
  usage() const
  {
    std::cout << "Usage:\n"
              << "  " << m_programName << " [options]\n"
              << "\n"
              << "Multiple prefixes can be configured for handling.\n"
              << "Set environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.\n"
              << "\n"
              << "Options:\n"
              << "  [-q]          - quiet mode: no interest reception/data generation logging\n"
              << "  [-h]          - print this help text and exit\n";
    exit(EXIT_FAILURE);
  }

  bool
  hasError() const
  {
    return m_hasError;
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
    m_logger.log("Total Interests Received    = " +
                 to_string(m_nInterestsReceived), false, true);
    m_logger.log("Total Responses Sent        = " +
                 to_string(m_nInterestsSent), false, true);
  }

  void
  onInterest(const Name& name, const Interest& interest)
  {
	  m_nInterestsReceived++;
	  const std::string interestName = interest.getName().toUri();
	  if (interestName.substr(0,3) != "IFA") {
	    Name dataName(interestName);
	    static const std::string content = "NC";

	    shared_ptr<Data> data = make_shared<Data>();
	    data->setName(dataName);
	    data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.size());

	    m_keyChain.sign(*data);
	    m_nInterestsSent++;

        if (!m_hasQuietLogging) {
          std::string logLine =
            "Interest Received          - GlobalID=" + to_string(m_nInterestsReceived) +
            ", Name=" + interestName;
          m_logger.log(logLine, true, false);
        }

        m_face.put(*data);
	  }
  }

  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason)
  {
    std::string logLine;
    logLine += "Prefix Registration Failed - Name=" + prefix.toUri();
    m_logger.log(logLine, true, true);

    m_nRegistrationsFailed++;
  }

  void
  run()
  {
    boost::asio::signal_set signalSet(m_ioService, SIGINT, SIGTERM);
    signalSet.async_wait(bind(&NdnProducer::signalHandler, this));

    m_logger.initializeLog(m_instanceId);

    m_face.setInterestFilter("/ndn",
                             bind(&NdnProducer::onInterest, this, _1, _2),
                             bind(&NdnProducer::onRegisterFailed, this, _1, _2));

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
  KeyChain m_keyChain;
  Logger m_logger;
  std::string m_programName;
  bool m_hasError;
  bool m_hasQuietLogging;
  std::size_t m_nRegistrationsFailed;
  int m_nInterestsSent;
  int m_nInterestsReceived;
  time::milliseconds m_contentDelay;
  std::string m_instanceId;
  std::string m_configurationFile;

  boost::asio::io_service m_ioService;
  Face m_face;
};

} // namespace ndn

int
main(int argc, char* argv[])
{
  std::srand(std::time(nullptr));

  ndn::NdnProducer server(argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hqc:d:")) != -1) {
    switch (option) {
    case 'h':
      server.usage();
      break;
    case 'q':
      server.setQuietLogging();
      break;
    default:
      server.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  server.run();

  return server.hasError() ? EXIT_FAILURE : EXIT_SUCCESS;
}
