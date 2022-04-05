/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022, Arizona Board of Regents.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include "util.hpp"

#include <ndn-cxx/data.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/time.hpp>

#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace po = boost::program_options;
using namespace ndn::time_literals;
using namespace std::string_literals;

namespace ndntg {

namespace time = ndn::time;

class NdnTrafficClient : boost::noncopyable
{
public:
  explicit
  NdnTrafficClient(const std::string& configFile)
    : m_signalSet(m_ioService, SIGINT, SIGTERM)
    , m_logger("NdnTrafficClient")
    , m_face(m_ioService)
    , m_configurationFile(configFile)
  {
  }

  void
  setMaximumInterests(uint64_t maxInterests)
  {
    m_nMaximumInterests = maxInterests;
  }

  void
  setInterestInterval(time::milliseconds interval)
  {
    BOOST_ASSERT(interval > 0_ms);
    m_interestInterval = interval;
  }

  void
  setQuietLogging()
  {
    m_wantQuiet = true;
  }

  int
  run()
  {
    m_logger.initializeLog(std::to_string(ndn::random::generateWord32()));

    if (!readConfigurationFile(m_configurationFile, m_trafficPatterns, m_logger)) {
      return 2;
    }

    if (!checkTrafficPatternCorrectness()) {
      m_logger.log("ERROR: Traffic configuration provided is not proper", false, true);
      return 2;
    }

    m_logger.log("Traffic configuration file processing completed.\n", true, false);
    for (std::size_t i = 0; i < m_trafficPatterns.size(); i++) {
      m_logger.log("Traffic Pattern Type #" + std::to_string(i + 1), false, false);
      m_trafficPatterns[i].printTrafficConfiguration(m_logger);
      m_logger.log("", false, false);
    }

    if (m_nMaximumInterests == 0) {
      logStatistics();
      return 0;
    }

    m_signalSet.async_wait([this] (auto&&...) { stop(); });

    boost::asio::deadline_timer timer(m_ioService,
                                      boost::posix_time::millisec(m_interestInterval.count()));
    timer.async_wait([this, &timer] (auto&&...) { generateTraffic(timer); });

    try {
      m_face.processEvents();
      return m_hasError ? 1 : 0;
    }
    catch (const std::exception& e) {
      m_logger.log("ERROR: "s + e.what(), true, true);
      m_ioService.stop();
      return 1;
    }
  }

private:
  class InterestTrafficConfiguration
  {
  public:
    void
    printTrafficConfiguration(Logger& logger) const
    {
      std::ostringstream os;

      os << "TrafficPercentage=" << m_trafficPercentage << ", ";
      os << "Name=" << m_name << ", ";
      if (m_nameAppendBytes) {
        os << "NameAppendBytes=" << *m_nameAppendBytes << ", ";
      }
      if (m_nameAppendSeqNum) {
        os << "NameAppendSequenceNumber=" << *m_nameAppendSeqNum << ", ";
      }
      if (m_canBePrefix) {
        os << "CanBePrefix=" << m_canBePrefix << ", ";
      }
      if (m_mustBeFresh) {
        os << "MustBeFresh=" << m_mustBeFresh << ", ";
      }
      if (m_nonceDuplicationPercentage > 0) {
        os << "NonceDuplicationPercentage=" << m_nonceDuplicationPercentage << ", ";
      }
      if (m_interestLifetime >= 0_ms) {
        os << "InterestLifetime=" << m_interestLifetime.count() << ", ";
      }
      if (m_nextHopFaceId > 0) {
        os << "NextHopFaceId=" << m_nextHopFaceId << ", ";
      }
      if (m_expectedContent) {
        os << "ExpectedContent=" << *m_expectedContent << ", ";
      }

      auto str = os.str();
      str = str.substr(0, str.length() - 2); // remove suffix ", "
      logger.log(str, false, false);
    }

    bool
    parseConfigurationLine(const std::string& line, Logger& logger, int lineNumber)
    {
      std::string parameter, value;
      if (!extractParameterAndValue(line, parameter, value)) {
        logger.log("Line " + std::to_string(lineNumber) + " - Invalid syntax: " + line,
                   false, true);
        return false;
      }

      if (parameter == "TrafficPercentage") {
        m_trafficPercentage = std::stoul(value);
      }
      else if (parameter == "Name") {
        m_name = value;
      }
      else if (parameter == "NameAppendBytes") {
        m_nameAppendBytes = std::stoul(value);
      }
      else if (parameter == "NameAppendSequenceNumber") {
        m_nameAppendSeqNum = std::stoull(value);
      }
      else if (parameter == "CanBePrefix") {
        m_canBePrefix = parseBoolean(value);
      }
      else if (parameter == "MustBeFresh") {
        m_mustBeFresh = parseBoolean(value);
      }
      else if (parameter == "NonceDuplicationPercentage") {
        m_nonceDuplicationPercentage = std::stoul(value);
      }
      else if (parameter == "InterestLifetime") {
        m_interestLifetime = time::milliseconds(std::stoul(value));
      }
      else if (parameter == "NextHopFaceId") {
        m_nextHopFaceId = std::stoull(value);
      }
      else if (parameter == "ExpectedContent") {
        m_expectedContent = value;
      }
      else {
        logger.log("Line " + std::to_string(lineNumber) + " - Ignoring unknown parameter: " + parameter,
                   false, true);
      }
      return true;
    }

    bool
    checkTrafficDetailCorrectness() const
    {
      return true;
    }

  public:
    uint8_t m_trafficPercentage = 0;
    std::string m_name;
    std::optional<std::size_t> m_nameAppendBytes;
    std::optional<uint64_t> m_nameAppendSeqNum;
    bool m_canBePrefix = false;
    bool m_mustBeFresh = false;
    uint8_t m_nonceDuplicationPercentage = 0;
    time::milliseconds m_interestLifetime = -1_ms;
    uint64_t m_nextHopFaceId = 0;
    std::optional<std::string> m_expectedContent;

    uint64_t m_nInterestsSent = 0;
    uint64_t m_nInterestsReceived = 0;
    uint64_t m_nNacks = 0;
    uint64_t m_nContentInconsistencies = 0;

    // RTT is stored as milliseconds with fractional sub-milliseconds precision
    double m_minimumInterestRoundTripTime = std::numeric_limits<double>::max();
    double m_maximumInterestRoundTripTime = 0;
    double m_totalInterestRoundTripTime = 0;
  };

  void
  logStatistics()
  {
    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Traffic Pattern Types = " +
                 std::to_string(m_trafficPatterns.size()), false, true);
    m_logger.log("Total Interests Sent        = " +
                 std::to_string(m_nInterestsSent), false, true);
    m_logger.log("Total Responses Received    = " +
                 std::to_string(m_nInterestsReceived), false, true);
    m_logger.log("Total Nacks Received        = " +
                 std::to_string(m_nNacks), false, true);

    double loss = 0.0;
    if (m_nInterestsSent > 0) {
      loss = (m_nInterestsSent - m_nInterestsReceived) * 100.0 / m_nInterestsSent;
    }
    m_logger.log("Total Interest Loss         = " + std::to_string(loss) + "%", false, true);

    double average = 0.0;
    double inconsistency = 0.0;
    if (m_nInterestsReceived > 0) {
      average = m_totalInterestRoundTripTime / m_nInterestsReceived;
      inconsistency = m_nContentInconsistencies * 100.0 / m_nInterestsReceived;
    }
    m_logger.log("Total Data Inconsistency    = " +
                 std::to_string(inconsistency) + "%", false, true);
    m_logger.log("Total Round Trip Time       = " +
                 std::to_string(m_totalInterestRoundTripTime) + "ms", false, true);
    m_logger.log("Average Round Trip Time     = " +
                 std::to_string(average) + "ms\n", false, true);

    for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++) {
      m_logger.log("Traffic Pattern Type #" + std::to_string(patternId + 1), false, true);
      m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
      m_logger.log("Total Interests Sent        = " +
                   std::to_string(m_trafficPatterns[patternId].m_nInterestsSent), false, true);
      m_logger.log("Total Responses Received    = " +
                   std::to_string(m_trafficPatterns[patternId].m_nInterestsReceived), false, true);
      m_logger.log("Total Nacks Received        = " +
                   std::to_string(m_trafficPatterns[patternId].m_nNacks), false, true);
      loss = 0;
      if (m_trafficPatterns[patternId].m_nInterestsSent > 0) {
        loss = m_trafficPatterns[patternId].m_nInterestsSent - m_trafficPatterns[patternId].m_nInterestsReceived;
        loss *= 100.0;
        loss /= m_trafficPatterns[patternId].m_nInterestsSent;
      }
      m_logger.log("Total Interest Loss         = " + std::to_string(loss) + "%", false, true);
      average = 0;
      inconsistency = 0;
      if (m_trafficPatterns[patternId].m_nInterestsReceived > 0) {
        average = m_trafficPatterns[patternId].m_totalInterestRoundTripTime /
                  m_trafficPatterns[patternId].m_nInterestsReceived;
        inconsistency = m_trafficPatterns[patternId].m_nContentInconsistencies;
        inconsistency *= 100.0 / m_trafficPatterns[patternId].m_nInterestsReceived;
      }
      m_logger.log("Total Data Inconsistency    = " + std::to_string(inconsistency) + "%", false, true);
      m_logger.log("Total Round Trip Time       = " +
                   std::to_string(m_trafficPatterns[patternId].m_totalInterestRoundTripTime) + "ms", false, true);
      m_logger.log("Average Round Trip Time     = " + std::to_string(average) + "ms\n", false, true);
    }
  }

  bool
  checkTrafficPatternCorrectness() const
  {
    // TODO
    return true;
  }

  uint32_t
  getNewNonce()
  {
    if (m_nonces.size() >= 1000)
      m_nonces.clear();

    auto randomNonce = ndn::random::generateWord32();
    while (std::find(m_nonces.begin(), m_nonces.end(), randomNonce) != m_nonces.end())
      randomNonce = ndn::random::generateWord32();

    m_nonces.push_back(randomNonce);
    return randomNonce;
  }

  uint32_t
  getOldNonce()
  {
    if (m_nonces.empty())
      return getNewNonce();

    std::uniform_int_distribution<std::size_t> dist(0, m_nonces.size() - 1);
    return m_nonces[dist(ndn::random::getRandomNumberEngine())];
  }

  static auto
  generateRandomNameComponent(std::size_t length)
  {
    // per ISO C++ std, cannot instantiate uniform_int_distribution with uint8_t
    static std::uniform_int_distribution<unsigned short> dist(std::numeric_limits<uint8_t>::min(),
                                                              std::numeric_limits<uint8_t>::max());

    ndn::Buffer buf(length);
    for (std::size_t i = 0; i < length; i++) {
      buf[i] = static_cast<uint8_t>(dist(ndn::random::getRandomNumberEngine()));
    }
    return ndn::name::Component(buf);
  }

  auto
  prepareInterest(std::size_t patternId)
  {
    ndn::Interest interest;
    auto& pattern = m_trafficPatterns[patternId];

    ndn::Name name(pattern.m_name);
    if (pattern.m_nameAppendBytes > 0) {
      name.append(generateRandomNameComponent(*pattern.m_nameAppendBytes));
    }
    if (pattern.m_nameAppendSeqNum) {
      auto seqNum = *pattern.m_nameAppendSeqNum;
      name.appendSequenceNumber(seqNum);
      pattern.m_nameAppendSeqNum = seqNum + 1;
    }
    interest.setName(name);

    interest.setCanBePrefix(pattern.m_canBePrefix);
    interest.setMustBeFresh(pattern.m_mustBeFresh);

    static std::uniform_int_distribution<> duplicateNonceDist(1, 100);
    if (duplicateNonceDist(ndn::random::getRandomNumberEngine()) <= pattern.m_nonceDuplicationPercentage)
      interest.setNonce(getOldNonce());
    else
      interest.setNonce(getNewNonce());

    if (pattern.m_interestLifetime >= 0_ms)
      interest.setInterestLifetime(pattern.m_interestLifetime);

    if (pattern.m_nextHopFaceId > 0)
      interest.setTag(std::make_shared<ndn::lp::NextHopFaceIdTag>(pattern.m_nextHopFaceId));

    return interest;
  }

  void
  onData(const ndn::Data& data, int globalRef, int localRef, std::size_t patternId,
         const time::steady_clock::time_point& sentTime)
  {
    auto logLine = "Data Received      - PatternType=" + std::to_string(patternId + 1) +
                   ", GlobalID=" + std::to_string(globalRef) +
                   ", LocalID=" + std::to_string(localRef) +
                   ", Name=" + data.getName().toUri();

    m_nInterestsReceived++;
    m_trafficPatterns[patternId].m_nInterestsReceived++;

    if (m_trafficPatterns[patternId].m_expectedContent) {
      std::string receivedContent = readString(data.getContent());
      if (receivedContent != *m_trafficPatterns[patternId].m_expectedContent) {
        m_nContentInconsistencies++;
        m_trafficPatterns[patternId].m_nContentInconsistencies++;
        logLine += ", IsConsistent=No";
      }
      else {
        logLine += ", IsConsistent=Yes";
      }
    }
    else {
      logLine += ", IsConsistent=NotChecked";
    }
    if (!m_wantQuiet) {
      m_logger.log(logLine, true, false);
    }

    double roundTripTime = (time::steady_clock::now() - sentTime).count() / 1000000.0;
    if (m_minimumInterestRoundTripTime > roundTripTime)
      m_minimumInterestRoundTripTime = roundTripTime;
    if (m_maximumInterestRoundTripTime < roundTripTime)
      m_maximumInterestRoundTripTime = roundTripTime;
    if (m_trafficPatterns[patternId].m_minimumInterestRoundTripTime > roundTripTime)
      m_trafficPatterns[patternId].m_minimumInterestRoundTripTime = roundTripTime;
    if (m_trafficPatterns[patternId].m_maximumInterestRoundTripTime < roundTripTime)
      m_trafficPatterns[patternId].m_maximumInterestRoundTripTime = roundTripTime;
    m_totalInterestRoundTripTime += roundTripTime;
    m_trafficPatterns[patternId].m_totalInterestRoundTripTime += roundTripTime;

    if (m_nMaximumInterests == globalRef) {
      stop();
    }
  }

  void
  onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack,
         int globalRef, int localRef, std::size_t patternId)
  {
    auto logLine = "Interest Nack'd    - PatternType=" + std::to_string(patternId + 1) +
                   ", GlobalID=" + std::to_string(globalRef) +
                   ", LocalID=" + std::to_string(localRef) +
                   ", Name=" + interest.getName().toUri() +
                   ", NackReason=" + boost::lexical_cast<std::string>(nack.getReason());
    m_logger.log(logLine, true, false);

    m_nNacks++;
    m_trafficPatterns[patternId].m_nNacks++;

    if (m_nMaximumInterests == globalRef) {
      stop();
    }
  }

  void
  onTimeout(const ndn::Interest& interest, int globalRef, int localRef, int patternId)
  {
    auto logLine = "Interest Timed Out - PatternType=" + std::to_string(patternId + 1) +
                   ", GlobalID=" + std::to_string(globalRef) +
                   ", LocalID=" + std::to_string(localRef) +
                   ", Name=" + interest.getName().toUri();
    m_logger.log(logLine, true, false);

    if (m_nMaximumInterests == globalRef) {
      stop();
    }
  }

  void
  generateTraffic(boost::asio::deadline_timer& timer)
  {
    if (m_nMaximumInterests && m_nInterestsSent >= *m_nMaximumInterests) {
      return;
    }

    static std::uniform_int_distribution<> trafficDist(1, 100);
    int trafficKey = trafficDist(ndn::random::getRandomNumberEngine());

    int cumulativePercentage = 0;
    std::size_t patternId = 0;
    for (; patternId < m_trafficPatterns.size(); patternId++) {
      cumulativePercentage += m_trafficPatterns[patternId].m_trafficPercentage;
      if (trafficKey <= cumulativePercentage) {
        auto interest = prepareInterest(patternId);
        try {
          m_nInterestsSent++;
          m_trafficPatterns[patternId].m_nInterestsSent++;
          auto sentTime = time::steady_clock::now();
          m_face.expressInterest(interest,
                                 bind(&NdnTrafficClient::onData, this, _2, m_nInterestsSent,
                                      m_trafficPatterns[patternId].m_nInterestsSent, patternId, sentTime),
                                 bind(&NdnTrafficClient::onNack, this, _1, _2, m_nInterestsSent,
                                      m_trafficPatterns[patternId].m_nInterestsSent, patternId),
                                 bind(&NdnTrafficClient::onTimeout, this, _1, m_nInterestsSent,
                                      m_trafficPatterns[patternId].m_nInterestsSent, patternId));

          if (!m_wantQuiet) {
            auto logLine = "Sending Interest   - PatternType=" + std::to_string(patternId + 1) +
                           ", GlobalID=" + std::to_string(m_nInterestsSent) +
                           ", LocalID=" + std::to_string(m_trafficPatterns[patternId].m_nInterestsSent) +
                           ", Name=" + interest.getName().toUri();
            m_logger.log(logLine, true, false);
          }

          timer.expires_at(timer.expires_at() + boost::posix_time::millisec(m_interestInterval.count()));
          timer.async_wait([this, &timer] (auto&&...) { generateTraffic(timer); });
        }
        catch (const std::exception& e) {
          m_logger.log("ERROR: "s + e.what(), true, true);
        }
        break;
      }
    }
    if (patternId == m_trafficPatterns.size()) {
      timer.expires_at(timer.expires_at() + boost::posix_time::millisec(m_interestInterval.count()));
      timer.async_wait([this, &timer] (auto&&...) { generateTraffic(timer); });
    }
  }

  void
  stop()
  {
    if (m_nContentInconsistencies > 0 || m_nInterestsSent != m_nInterestsReceived) {
      m_hasError = true;
    }

    logStatistics();
    m_face.shutdown();
    m_ioService.stop();
  }

private:
  boost::asio::io_service m_ioService;
  boost::asio::signal_set m_signalSet;
  Logger m_logger;
  ndn::Face m_face;

  std::string m_configurationFile;
  std::optional<uint64_t> m_nMaximumInterests;
  time::milliseconds m_interestInterval = 1_s;

  std::vector<InterestTrafficConfiguration> m_trafficPatterns;
  std::vector<uint32_t> m_nonces;
  uint64_t m_nInterestsSent = 0;
  uint64_t m_nInterestsReceived = 0;
  uint64_t m_nNacks = 0;
  uint64_t m_nContentInconsistencies = 0;

  // RTT is stored as milliseconds with fractional sub-milliseconds precision
  double m_minimumInterestRoundTripTime = std::numeric_limits<double>::max();
  double m_maximumInterestRoundTripTime = 0;
  double m_totalInterestRoundTripTime = 0;

  bool m_wantQuiet = false;
  bool m_hasError = false;
};

} // namespace ndntg

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& desc)
{
  os << "Usage: " << programName << " [options] <Traffic_Configuration_File>\n"
     << "\n"
     << "Generate Interest traffic as per provided Traffic_Configuration_File.\n"
     << "Interests are continuously generated unless a total number is specified.\n"
     << "Set the environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.\n"
     << "\n"
     << desc;
}

int
main(int argc, char* argv[])
{
  std::string configFile;

  po::options_description visibleOptions("Options");
  visibleOptions.add_options()
    ("help,h",      "print this help message and exit")
    ("count,c",     po::value<int>(), "total number of Interests to be generated")
    ("interval,i",  po::value<ndn::time::milliseconds::rep>()->default_value(1000),
                    "Interest generation interval in milliseconds")
    ("quiet,q",     po::bool_switch(), "turn off logging of Interest generation/Data reception")
    ;

  po::options_description hiddenOptions;
  hiddenOptions.add_options()
    ("config-file", po::value<std::string>(&configFile))
    ;

  po::positional_options_description posOptions;
  posOptions.add("config-file", -1);

  po::options_description allOptions;
  allOptions.add(visibleOptions).add(hiddenOptions);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(allOptions).positional(posOptions).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  if (vm.count("help") > 0) {
    usage(std::cout, argv[0], visibleOptions);
    return 0;
  }

  if (configFile.empty()) {
    usage(std::cerr, argv[0], visibleOptions);
    return 2;
  }

  ndntg::NdnTrafficClient client(configFile);

  if (vm.count("count") > 0) {
    int count = vm["count"].as<int>();
    if (count < 0) {
      std::cerr << "ERROR: the argument for option '--count' cannot be negative" << std::endl;
      return 2;
    }
    client.setMaximumInterests(static_cast<uint64_t>(count));
  }

  if (vm.count("interval") > 0) {
    ndn::time::milliseconds interval(vm["interval"].as<ndn::time::milliseconds::rep>());
    if (interval <= 0_ms) {
      std::cerr << "ERROR: the argument for option '--interval' must be positive" << std::endl;
      return 2;
    }
    client.setInterestInterval(interval);
  }

  if (vm["quiet"].as<bool>()) {
    client.setQuietLogging();
  }

  return client.run();
}
