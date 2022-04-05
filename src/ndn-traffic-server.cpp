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
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/time.hpp>

#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/noncopyable.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/thread/thread.hpp>

namespace po = boost::program_options;
using namespace ndn::time_literals;
using namespace std::string_literals;

namespace ndntg {

namespace time = ndn::time;

class NdnTrafficServer : boost::noncopyable
{
public:
  explicit
  NdnTrafficServer(const std::string& configFile)
    : m_signalSet(m_ioService, SIGINT, SIGTERM)
    , m_logger("NdnTrafficServer")
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
  setContentDelay(time::milliseconds delay)
  {
    BOOST_ASSERT(delay >= 0_ms);
    m_contentDelay = delay;
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

    m_signalSet.async_wait([this] (const boost::system::error_code&, int) {
      if (m_nMaximumInterests && m_nInterestsReceived < *m_nMaximumInterests) {
        m_hasError = true;
      }
      stop();
    });

    for (std::size_t id = 0; id < m_trafficPatterns.size(); id++) {
      m_registeredPrefixes.push_back(
        m_face.setInterestFilter(m_trafficPatterns[id].m_name,
                                 [this, id] (auto&&, const auto& interest) { onInterest(interest, id); },
                                 nullptr,
                                 [this, id] (auto&&, const auto& reason) { onRegisterFailed(reason, id); }));
    }

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
  class DataTrafficConfiguration
  {
  public:
    void
    printTrafficConfiguration(Logger& logger) const
    {
      std::ostringstream os;

      if (!m_name.empty()) {
        os << "Name=" << m_name << ", ";
      }
      if (m_contentDelay >= 0_ms) {
        os << "ContentDelay=" << m_contentDelay.count() << ", ";
      }
      if (m_freshnessPeriod >= 0_ms) {
        os << "FreshnessPeriod=" << m_freshnessPeriod.count() << ", ";
      }
      if (m_contentType) {
        os << "ContentType=" << *m_contentType << ", ";
      }
      if (m_contentLength) {
        os << "ContentBytes=" << *m_contentLength << ", ";
      }
      if (!m_content.empty()) {
        os << "Content=" << m_content << ", ";
      }
      os << "SigningInfo=" << m_signingInfo;

      logger.log(os.str(), false, false);
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

      if (parameter == "Name") {
        m_name = value;
      }
      else if (parameter == "ContentDelay") {
        m_contentDelay = time::milliseconds(std::stoul(value));
      }
      else if (parameter == "FreshnessPeriod") {
        m_freshnessPeriod = time::milliseconds(std::stoul(value));
      }
      else if (parameter == "ContentType") {
        m_contentType = std::stoul(value);
      }
      else if (parameter == "ContentBytes") {
        m_contentLength = std::stoul(value);
      }
      else if (parameter == "Content") {
        m_content = value;
      }
      else if (parameter == "SigningInfo") {
        m_signingInfo = ndn::security::SigningInfo(value);
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
    std::string m_name;
    time::milliseconds m_contentDelay = -1_ms;
    time::milliseconds m_freshnessPeriod = -1_ms;
    std::optional<uint32_t> m_contentType;
    std::optional<std::size_t> m_contentLength;
    std::string m_content;
    ndn::security::SigningInfo m_signingInfo;
    uint64_t m_nInterestsReceived = 0;
  };

  void
  logStatistics()
  {
    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Traffic Pattern Types = " +
                 std::to_string(m_trafficPatterns.size()), false, true);
    m_logger.log("Total Interests Received    = " +
                 std::to_string(m_nInterestsReceived), false, true);

    for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++) {
      m_logger.log("\nTraffic Pattern Type #" + std::to_string(patternId + 1), false, true);
      m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
      m_logger.log("Total Interests Received    = " +
                   std::to_string(m_trafficPatterns[patternId].m_nInterestsReceived) + "\n", false, true);
    }
  }

  bool
  checkTrafficPatternCorrectness() const
  {
    // TODO
    return true;
  }

  static std::string
  getRandomByteString(std::size_t length)
  {
    // per ISO C++ std, cannot instantiate uniform_int_distribution with char
    static std::uniform_int_distribution<short> dist(std::numeric_limits<char>::min(),
                                                     std::numeric_limits<char>::max());

    std::string s;
    s.reserve(length);
    for (std::size_t i = 0; i < length; i++) {
      s += static_cast<char>(dist(ndn::random::getRandomNumberEngine()));
    }
    return s;
  }

  void
  onInterest(const ndn::Interest& interest, std::size_t patternId)
  {
    auto& pattern = m_trafficPatterns[patternId];

    if (!m_nMaximumInterests || m_nInterestsReceived < *m_nMaximumInterests) {
      ndn::Data data(interest.getName());

      if (pattern.m_freshnessPeriod >= 0_ms)
        data.setFreshnessPeriod(pattern.m_freshnessPeriod);

      if (pattern.m_contentType)
        data.setContentType(*pattern.m_contentType);

      std::string content;
      if (pattern.m_contentLength > 0)
        content = getRandomByteString(*pattern.m_contentLength);
      if (!pattern.m_content.empty())
        content = pattern.m_content;
      data.setContent(ndn::makeStringBlock(ndn::tlv::Content, content));

      m_keyChain.sign(data, pattern.m_signingInfo);

      m_nInterestsReceived++;
      pattern.m_nInterestsReceived++;

      if (!m_wantQuiet) {
        auto logLine = "Interest received          - PatternType=" + std::to_string(patternId + 1) +
                       ", GlobalID=" + std::to_string(m_nInterestsReceived) +
                       ", LocalID=" + std::to_string(pattern.m_nInterestsReceived) +
                       ", Name=" + pattern.m_name;
        m_logger.log(logLine, true, false);
      }

      if (pattern.m_contentDelay > 0_ms)
        boost::this_thread::sleep_for(pattern.m_contentDelay);
      if (m_contentDelay > 0_ms)
        boost::this_thread::sleep_for(m_contentDelay);

      m_face.put(data);
    }

    if (m_nMaximumInterests && m_nInterestsReceived >= *m_nMaximumInterests) {
      logStatistics();
      m_registeredPrefixes.clear();
      m_signalSet.cancel();
    }
  }

  void
  onRegisterFailed(const std::string& reason, std::size_t patternId)
  {
    auto logLine = "Prefix registration failed - PatternType=" + std::to_string(patternId + 1) +
                   ", Name=" + m_trafficPatterns[patternId].m_name +
                   ", Reason=" + reason;
    m_logger.log(logLine, true, true);

    m_nRegistrationsFailed++;
    if (m_nRegistrationsFailed == m_trafficPatterns.size()) {
      m_hasError = true;
      stop();
    }
  }

  void
  stop()
  {
    logStatistics();
    m_face.shutdown();
    m_ioService.stop();
  }

private:
  boost::asio::io_service m_ioService;
  boost::asio::signal_set m_signalSet;
  Logger m_logger;
  ndn::Face m_face;
  ndn::KeyChain m_keyChain;

  std::string m_configurationFile;
  std::optional<uint64_t> m_nMaximumInterests;
  time::milliseconds m_contentDelay = 0_ms;

  std::vector<DataTrafficConfiguration> m_trafficPatterns;
  std::vector<ndn::ScopedRegisteredPrefixHandle> m_registeredPrefixes;
  uint64_t m_nRegistrationsFailed = 0;
  uint64_t m_nInterestsReceived = 0;

  bool m_wantQuiet = false;
  bool m_hasError = false;
};

} // namespace ndntg

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& desc)
{
  os << "Usage: " << programName << " [options] <Traffic_Configuration_File>\n"
     << "\n"
     << "Respond to Interests as per provided Traffic_Configuration_File.\n"
     << "Multiple prefixes can be configured for handling.\n"
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
    ("help,h",    "print this help message and exit")
    ("count,c",   po::value<int>(), "maximum number of Interests to respond to")
    ("delay,d",   po::value<ndn::time::milliseconds::rep>()->default_value(0),
                  "wait this amount of milliseconds before responding to each Interest")
    ("quiet,q",   po::bool_switch(), "turn off logging of Interest reception/Data generation")
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

  ndntg::NdnTrafficServer server(configFile);

  if (vm.count("count") > 0) {
    int count = vm["count"].as<int>();
    if (count < 0) {
      std::cerr << "ERROR: the argument for option '--count' cannot be negative" << std::endl;
      return 2;
    }
    server.setMaximumInterests(static_cast<uint64_t>(count));
  }

  if (vm.count("delay") > 0) {
    ndn::time::milliseconds delay(vm["delay"].as<ndn::time::milliseconds::rep>());
    if (delay < 0_ms) {
      std::cerr << "ERROR: the argument for option '--delay' cannot be negative" << std::endl;
      return 2;
    }
    server.setContentDelay(delay);
  }

  if (vm["quiet"].as<bool>()) {
    server.setQuietLogging();
  }

  return server.run();
}
