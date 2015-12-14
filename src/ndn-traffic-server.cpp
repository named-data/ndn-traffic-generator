/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2014-2015  University of Arizona.
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

class NdnTrafficServer : boost::noncopyable
{
public:
  explicit
  NdnTrafficServer(const char* programName)
    : m_logger("NdnTrafficServer")
    , m_programName(programName)
    , m_hasError(false)
    , m_hasQuietLogging(false)
    , m_nRegistrationsFailed(0)
    , m_nMaximumInterests(-1)
    , m_nInterestsReceived(0)
    , m_contentDelay(time::milliseconds(-1))
    , m_instanceId(to_string(std::rand()))
    , m_face(m_ioService)
  {
  }

  class DataTrafficConfiguration
  {
  public:
    DataTrafficConfiguration()
      : m_contentType(-1)
      , m_freshnessPeriod(time::milliseconds(-1))
      , m_contentBytes(-1)
      , m_contentDelay(time::milliseconds(-1))
      , m_nInterestsReceived(0)
      , m_signingInfo()
    {
    }

    void
    printTrafficConfiguration(Logger& logger)
    {
      std::stringstream detail;

      if (!m_name.empty())
        detail << "Name=" << m_name << ", ";
      if (m_contentType >= 0)
        detail << "ContentType=" << to_string(m_contentType) << ", ";
      if (m_freshnessPeriod >= time::milliseconds(0))
        detail << "FreshnessPeriod=" <<
                  to_string(static_cast<int>(m_freshnessPeriod.count())) << ", ";
      if (m_contentBytes >= 0)
        detail << "ContentBytes=" << to_string(m_contentBytes) << ", ";
      if (m_contentDelay >= time::milliseconds(0))
        detail << "ContentDelay=" << to_string(m_contentDelay.count()) << ", ";
      if (!m_content.empty())
        detail << "Content=" << m_content << ", ";
      detail << "SigningInfo=" << m_signingInfo;

      logger.log(detail.str(), false, false);
    }

    bool
    extractParameterValue(const std::string& detail, std::string& parameter, std::string& value)
    {
      std::string allowedCharacters = ":/+._-%";
      std::size_t i = 0;

      parameter = "";
      value = "";
      while (detail[i] != '=' && i < detail.length()) {
        parameter += detail[i];
        i++;
      }
      if (i == detail.length())
        return false;

      i++;
      while ((std::isalnum(detail[i]) ||
              allowedCharacters.find(detail[i]) != std::string::npos) &&
             i < detail.length()) {
        value += detail[i];
        i++;
      }

      if (parameter.empty() || value.empty())
        return false;
      else
        return true;
    }

    bool
    processConfigurationDetail(const std::string& detail,
                               Logger& logger,
                               int lineNumber)
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value)) {
        if (parameter == "Name") {
          m_name = value;
        }
        else if (parameter == "ContentType") {
          m_contentType = std::stoi(value);
        }
        else if (parameter == "FreshnessPeriod") {
          m_freshnessPeriod = time::milliseconds(std::stoi(value));
        }
        else if (parameter == "ContentDelay") {
          m_contentDelay = time::milliseconds(std::stoi(value));
        }
        else if (parameter == "ContentBytes") {
          m_contentBytes = std::stoi(value);
        }
        else if (parameter == "Content") {
          m_content = value;
        }
        else if (parameter == "SigningInfo") {
          m_signingInfo = security::SigningInfo(value);
        }
        else {
          logger.log("Line " + to_string(lineNumber) +
                     " \t- Invalid Parameter='" + parameter + "'", false, true);
        }
      }
      else {
        logger.log("Line " + to_string(lineNumber) +
                   " \t- Improper Traffic Configuration Line - " + detail, false, true);
        return false;
      }

      return true;
    }

    bool
    checkTrafficDetailCorrectness()
    {
      return true;
    }

  private:
    std::string m_name;
    int m_contentType;
    time::milliseconds m_freshnessPeriod;
    int m_contentBytes;
    time::milliseconds m_contentDelay;
    std::string m_content;
    int m_nInterestsReceived;
    security::SigningInfo m_signingInfo;

    friend class NdnTrafficServer;
  };

  void
  usage() const
  {
    std::cout << "Usage:\n"
              << "  " << m_programName << " [options] <Traffic_Configuration_File>\n"
              << "\n"
              << "Respond to Interests as per provided Traffic Configuration File.\n"
              << "Multiple prefixes can be configured for handling.\n"
              << "Set environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.\n"
              << "\n"
              << "Options:\n"
              << "  [-d interval] - set delay before responding to interest, in milliseconds\n"
              << "  [-c count]    - specify maximum number of interests to be satisfied\n"
              << "  [-q]          - quiet mode: no interest reception/data generation logging\n"
              << "  [-h]          - print this help text and exit\n";
    exit(EXIT_FAILURE);
  }

  void
  setMaximumInterests(int maximumInterests)
  {
    if (maximumInterests < 0)
      usage();
    m_nMaximumInterests = maximumInterests;
  }

  bool
  hasError() const
  {
    return m_hasError;
  }

  void
  setContentDelay(int contentDelay)
  {
    if (contentDelay < 0)
      usage();
    m_contentDelay = time::milliseconds(contentDelay);
  }

  void
  setConfigurationFile(const char* configurationFile)
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
    m_logger.log("Total Traffic Pattern Types = " +
                 to_string(m_trafficPatterns.size()), false, true);
    m_logger.log("Total Interests Received    = " +
                 to_string(m_nInterestsReceived), false, true);

    if (m_nInterestsReceived < m_nMaximumInterests)
      m_hasError = true;

    for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      {
        m_logger.log("\nTraffic Pattern Type #" + to_string(patternId + 1), false, true);
        m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Received    = " + to_string(
                       m_trafficPatterns[patternId].m_nInterestsReceived) + "\n", false, true);
      }
  }

  bool
  checkTrafficPatternCorrectness()
  {
    return true;
  }

  void
  parseConfigurationFile()
  {
    std::string patternLine;
    std::ifstream patternFile;
    m_logger.log("Analyzing Traffic Configuration File: " + m_configurationFile, true, true);

    patternFile.open(m_configurationFile.c_str());
    if (patternFile.is_open())
      {
        int lineNumber = 0;
        while (getline(patternFile, patternLine))
          {
            lineNumber++;
            if (std::isalpha(patternLine[0]))
              {
                DataTrafficConfiguration dataData;
                bool shouldSkipLine = false;
                if (dataData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                  {
                    while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
                      {
                        lineNumber++;
                        if (!dataData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                          {
                            shouldSkipLine = true;
                            break;
                          }
                      }
                    lineNumber++;
                  }
                else
                  shouldSkipLine = true;
                if (!shouldSkipLine)
                  {
                    if (dataData.checkTrafficDetailCorrectness())
                      m_trafficPatterns.push_back(dataData);
                  }
              }
          }
        patternFile.close();

        if (!checkTrafficPatternCorrectness())
          {
            m_logger.log("ERROR - Traffic Configuration Provided Is Not Proper - " +
                         m_configurationFile, false, true);
            m_logger.shutdownLogger();
            exit(EXIT_FAILURE);
          }

        m_logger.log("Traffic Configuration File Processing Completed\n", true, false);
        for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
          {
            m_logger.log("Traffic Pattern Type #" +
                         to_string(patternId + 1), false, false);
            m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
            m_logger.log("", false, false);
          }
      }
    else
      {
        m_logger.log("ERROR - Unable To Open Traffic Configuration File: " +
          m_configurationFile, false, true);
        m_logger.shutdownLogger();
        exit(EXIT_FAILURE);
      }
  }

  void
  initializeTrafficConfiguration()
  {
    if (boost::filesystem::exists(boost::filesystem::path(m_configurationFile)))
      {
        if (boost::filesystem::is_regular_file(boost::filesystem::path(m_configurationFile)))
          {
            parseConfigurationFile();
          }
        else
          {
            m_logger.log("ERROR - Traffic Configuration File Is Not A Regular File: " +
              m_configurationFile, false, true);
            m_logger.shutdownLogger();
            exit(EXIT_FAILURE);
          }
      }
    else
      {
        m_logger.log("ERROR - Traffic Configuration File Does Not Exist: " +
          m_configurationFile, false, true);
        m_logger.shutdownLogger();
        exit(EXIT_FAILURE);
      }
  }

  static std::string
  getRandomByteString(std::size_t randomSize)
  {
    std::string randomString;
    for (std::size_t i = 0; i < randomSize; i++)
      randomString += static_cast<char>(std::rand() % 128);
    return randomString;
  }

  void
  onInterest(const Name& name, const Interest& interest, int patternId)
  {
    auto& pattern = m_trafficPatterns[patternId];

    if (m_nMaximumInterests < 0 || m_nInterestsReceived < m_nMaximumInterests)
      {
        Data data(interest.getName());

        if (pattern.m_contentType >= 0)
          data.setContentType(pattern.m_contentType);

        if (pattern.m_freshnessPeriod >= time::milliseconds(0))
          data.setFreshnessPeriod(pattern.m_freshnessPeriod);

        std::string content;
        if (pattern.m_contentBytes >= 0)
          content = getRandomByteString(pattern.m_contentBytes);
        if (!pattern.m_content.empty())
          content = pattern.m_content;

        data.setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
        m_keyChain.sign(data, pattern.m_signingInfo);

        m_nInterestsReceived++;
        pattern.m_nInterestsReceived++;

        if (!m_hasQuietLogging) {
          std::string logLine =
            "Interest Received          - PatternType=" + to_string(patternId + 1) +
            ", GlobalID=" + to_string(m_nInterestsReceived) +
            ", LocalID=" + to_string(pattern.m_nInterestsReceived) +
            ", Name=" + pattern.m_name;
          m_logger.log(logLine, true, false);
        }

        if (pattern.m_contentDelay > time::milliseconds(-1))
          usleep(pattern.m_contentDelay.count() * 1000);
        if (m_contentDelay > time::milliseconds(-1))
          usleep(m_contentDelay.count() * 1000);
        m_face.put(data);
      }
    if (m_nMaximumInterests >= 0 && m_nInterestsReceived == m_nMaximumInterests)
      {
        logStatistics();
        m_logger.shutdownLogger();
        m_face.shutdown();
        m_ioService.stop();
      }
  }

  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason, int patternId)
  {
    std::string logLine;
    logLine += "Prefix Registration Failed - PatternType=" + to_string(patternId + 1);
    logLine += ", Name=" + m_trafficPatterns[patternId].m_name;
    m_logger.log(logLine, true, true);

    m_nRegistrationsFailed++;
    if (m_nRegistrationsFailed == m_trafficPatterns.size())
      {
        m_hasError = true;
        signalHandler();
      }
  }

  void
  run()
  {
    boost::asio::signal_set signalSet(m_ioService, SIGINT, SIGTERM);
    signalSet.async_wait(bind(&NdnTrafficServer::signalHandler, this));

    m_logger.initializeLog(m_instanceId);
    initializeTrafficConfiguration();
    if (m_nMaximumInterests == 0)
      {
        logStatistics();
        m_logger.shutdownLogger();
        return;
      }

    for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      m_face.setInterestFilter(m_trafficPatterns[patternId].m_name,
                               bind(&NdnTrafficServer::onInterest, this, _1, _2, patternId),
                               bind(&NdnTrafficServer::onRegisterFailed, this, _1, _2, patternId));

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
  int m_nMaximumInterests;
  int m_nInterestsReceived;
  time::milliseconds m_contentDelay;
  std::string m_instanceId;
  std::string m_configurationFile;

  boost::asio::io_service m_ioService;
  Face m_face;
  std::vector<DataTrafficConfiguration> m_trafficPatterns;
};

} // namespace ndn

int
main(int argc, char* argv[])
{
  std::srand(std::time(nullptr));

  ndn::NdnTrafficServer server(argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hqc:d:")) != -1) {
    switch (option) {
    case 'h':
      server.usage();
      break;
    case 'c':
      server.setMaximumInterests(atoi(optarg));
      break;
    case 'd':
      server.setContentDelay(atoi(optarg));
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

  if (!argc)
    server.usage();

  server.setConfigurationFile(argv[0]);
  server.run();

  return server.hasError() ? EXIT_FAILURE : EXIT_SUCCESS;
}