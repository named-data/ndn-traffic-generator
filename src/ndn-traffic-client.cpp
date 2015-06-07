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

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

#include <ndn-cxx/exclude.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/name-component.hpp>

#include "logger.hpp"

namespace ndn {

class NdnTrafficClient : boost::noncopyable
{
public:
  explicit
  NdnTrafficClient(const char* programName)
    : m_programName(programName)
    , m_logger("NdnTrafficClient")
    , m_instanceId(std::to_string(std::rand()))
    , m_hasError(false)
    , m_hasQuietLogging(false)
    , m_interestInterval(getDefaultInterestInterval())
    , m_nMaximumInterests(-1)
    , m_face(m_ioService)
    , m_nInterestsSent(0)
    , m_nInterestsReceived(0)
    , m_nContentInconsistencies(0)
    , m_minimumInterestRoundTripTime(std::numeric_limits<double>::max())
    , m_maximumInterestRoundTripTime(0)
    , m_totalInterestRoundTripTime(0)
  {
  }

  class InterestTrafficConfiguration
  {
  public:
    InterestTrafficConfiguration()
      : m_trafficPercentage(-1)
      , m_nameAppendBytes(-1)
      , m_nameAppendSequenceNumber(-1)
      , m_minSuffixComponents(-1)
      , m_maxSuffixComponents(-1)
      , m_excludeBeforeBytes(-1)
      , m_excludeAfterBytes(-1)
      , m_childSelector(-1)
      , m_mustBeFresh(-1)
      , m_nonceDuplicationPercentage(-1)
      , m_interestLifetime(getDefaultInterestLifetime())
      , m_nInterestsSent(0)
      , m_nInterestsReceived(0)
      , m_minimumInterestRoundTripTime(std::numeric_limits<double>::max())
      , m_maximumInterestRoundTripTime(0)
      , m_totalInterestRoundTripTime(0)
      , m_nContentInconsistencies(0)
    {
    }

    static time::milliseconds
    getDefaultInterestLifetime()
    {
      return time::milliseconds(-1);
    }

    void
    printTrafficConfiguration(Logger& logger)
    {
      std::string detail;

      if (m_trafficPercentage > 0)
        detail += "TrafficPercentage=" + std::to_string(m_trafficPercentage) + ", ";
      if (!m_name.empty())
        detail += "Name=" + m_name + ", ";
      if (m_nameAppendBytes > 0)
        detail += "NameAppendBytes=" + std::to_string(m_nameAppendBytes) + ", ";
      if (m_nameAppendSequenceNumber > 0)
        detail += "NameAppendSequenceNumber=" + std::to_string(m_nameAppendSequenceNumber) + ", ";
      if (m_minSuffixComponents >= 0)
        detail += "MinSuffixComponents=" + std::to_string(m_minSuffixComponents) + ", ";
      if (m_maxSuffixComponents >= 0)
        detail += "MaxSuffixComponents=" + std::to_string(m_maxSuffixComponents) + ", ";
      if (!m_excludeBefore.empty())
        detail += "ExcludeBefore=" + m_excludeBefore + ", ";
      if (!m_excludeAfter.empty())
        detail += "ExcludeAfter=" + m_excludeAfter + ", ";
      if (m_excludeBeforeBytes > 0)
        detail += "ExcludeBeforeBytes=" + std::to_string(m_excludeBeforeBytes) + ", ";
      if (m_excludeAfterBytes > 0)
        detail += "ExcludeAfterBytes=" + std::to_string(m_excludeAfterBytes) + ", ";
      if (m_childSelector >= 0)
        detail += "ChildSelector=" + std::to_string(m_childSelector) + ", ";
      if (m_mustBeFresh >= 0)
        detail += "MustBeFresh=" + std::to_string(m_mustBeFresh) + ", ";
      if (m_nonceDuplicationPercentage > 0)
        detail += "NonceDuplicationPercentage=" +
                  std::to_string(m_nonceDuplicationPercentage) + ", ";
      if (m_interestLifetime >= time::milliseconds(0))
        detail += "InterestLifetime=" + std::to_string(m_interestLifetime.count()) + ", ";
      if (!m_expectedContent.empty())
        detail += "ExpectedContent=" + m_expectedContent + ", ";
      if (detail.length() >= 2)
        detail = detail.substr(0, detail.length() - 2); // Removing suffix ", "

      logger.log(detail, false, false);
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
    processConfigurationDetail(const std::string& detail, Logger& logger, int lineNumber)
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value))
        {
          if (parameter == "TrafficPercentage")
            m_trafficPercentage = std::stoi(value);
          else if (parameter == "Name")
            m_name = value;
          else if (parameter == "NameAppendBytes")
            m_nameAppendBytes = std::stoi(value);
          else if (parameter == "NameAppendSequenceNumber")
            m_nameAppendSequenceNumber = std::stoi(value);
          else if (parameter == "MinSuffixComponents")
            m_minSuffixComponents = std::stoi(value);
          else if (parameter == "MaxSuffixComponents")
            m_maxSuffixComponents = std::stoi(value);
          else if (parameter == "ExcludeBefore")
            m_excludeBefore = value;
          else if (parameter == "ExcludeAfter")
            m_excludeAfter = value;
          else if (parameter == "ExcludeBeforeBytes")
            m_excludeBeforeBytes = std::stoi(value);
          else if (parameter == "ExcludeAfterBytes")
            m_excludeAfterBytes = std::stoi(value);
          else if (parameter == "ChildSelector")
            m_childSelector = std::stoi(value);
          else if (parameter == "MustBeFresh")
            m_mustBeFresh = std::stoi(value);
          else if (parameter == "NonceDuplicationPercentage")
            m_nonceDuplicationPercentage = std::stoi(value);
          else if (parameter == "InterestLifetime")
            m_interestLifetime = time::milliseconds(std::stoi(value));
          else if (parameter == "ExpectedContent")
            m_expectedContent = value;
          else
            logger.log("Line " + std::to_string(lineNumber) +
                       " \t- Invalid Parameter='" + parameter + "'", false, true);
        }
      else
        {
          logger.log("Line " + std::to_string(lineNumber) +
                     " \t- Improper Traffic Configuration Line- " + detail, false, true);
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
    int m_trafficPercentage;
    std::string m_name;
    int m_nameAppendBytes;
    int m_nameAppendSequenceNumber;
    int m_minSuffixComponents;
    int m_maxSuffixComponents;
    std::string m_excludeBefore;
    std::string m_excludeAfter;
    int m_excludeBeforeBytes;
    int m_excludeAfterBytes;
    int m_childSelector;
    int m_mustBeFresh;
    int m_nonceDuplicationPercentage;
    time::milliseconds m_interestLifetime;
    int m_nInterestsSent;
    int m_nInterestsReceived;

    //round trip time is stored as milliseconds with fractional
    //sub-millisecond precision
    double m_minimumInterestRoundTripTime;
    double m_maximumInterestRoundTripTime;
    double m_totalInterestRoundTripTime;

    int m_nContentInconsistencies;
    std::string m_expectedContent;

    friend class NdnTrafficClient;
  }; // class InterestTrafficConfiguration

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
              << "  [-i interval] - set interest generation interval in milliseconds (default "
              << getDefaultInterestInterval() << ")\n"
              << "  [-c count]    - set total number of interests to be generated\n"
              << "  [-q]          - quiet mode: no interest reception/data generation logging\n"
              << "  [-h]          - print this help text and exit\n";
    exit(EXIT_FAILURE);
  }

  static time::milliseconds
  getDefaultInterestInterval()
  {
    return time::milliseconds(1000);
  }

  void
  setInterestInterval(int interestInterval)
  {
    if (interestInterval <= 0)
      usage();
    m_interestInterval = time::milliseconds(interestInterval);
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
    m_logger.log("Total Traffic Pattern Types = " +
      std::to_string(m_trafficPatterns.size()), false, true);
    m_logger.log("Total Interests Sent        = " +
      std::to_string(m_nInterestsSent), false, true);
    m_logger.log("Total Responses Received    = " +
      std::to_string(m_nInterestsReceived), false, true);

    double loss = 0;
    if (m_nInterestsSent > 0)
      loss = (m_nInterestsSent - m_nInterestsReceived) * 100.0 / m_nInterestsSent;
    m_logger.log("Total Interest Loss         = " + std::to_string(loss) + "%", false, true);
    if (m_nContentInconsistencies != 0 || m_nInterestsSent != m_nInterestsReceived)
      m_hasError = true;

    double average = 0;
    double inconsistency = 0;
    if (m_nInterestsReceived > 0)
      {
        average = m_totalInterestRoundTripTime / m_nInterestsReceived;
        inconsistency = m_nContentInconsistencies * 100.0 / m_nInterestsReceived;
      }
    m_logger.log("Total Data Inconsistency    = " +
      std::to_string(inconsistency) + "%", false, true);
    m_logger.log("Total Round Trip Time       = " +
      std::to_string(m_totalInterestRoundTripTime) + "ms", false, true);
    m_logger.log("Average Round Trip Time     = " +
      std::to_string(average) + "ms\n", false, true);

    for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      {
        m_logger.log("Traffic Pattern Type #" +
          std::to_string(patternId + 1), false, true);
        m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Sent        = " +
          std::to_string(m_trafficPatterns[patternId].m_nInterestsSent), false, true);
        m_logger.log("Total Responses Received    = " +
          std::to_string(m_trafficPatterns[patternId].m_nInterestsReceived), false, true);
        loss = 0;
        if (m_trafficPatterns[patternId].m_nInterestsSent > 0)
          {
            loss = (m_trafficPatterns[patternId].m_nInterestsSent -
                    m_trafficPatterns[patternId].m_nInterestsReceived);
            loss *= 100.0;
            loss /= m_trafficPatterns[patternId].m_nInterestsSent;
          }
        m_logger.log("Total Interest Loss         = " + std::to_string(loss) + "%", false, true);
        average = 0;
        inconsistency = 0;
        if (m_trafficPatterns[patternId].m_nInterestsReceived > 0)
          {
            average = (m_trafficPatterns[patternId].m_totalInterestRoundTripTime /
                       m_trafficPatterns[patternId].m_nInterestsReceived);
            inconsistency = m_trafficPatterns[patternId].m_nContentInconsistencies;
            inconsistency *= 100.0 / m_trafficPatterns[patternId].m_nInterestsReceived;
          }
        m_logger.log("Total Data Inconsistency    = " +
          std::to_string(inconsistency) + "%", false, true);
        m_logger.log("Total Round Trip Time       = " +
          std::to_string(m_trafficPatterns[patternId].m_totalInterestRoundTripTime) +
          "ms", false, true);
        m_logger.log("Average Round Trip Time     = " +
          std::to_string(average) + "ms\n", false, true);
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
                InterestTrafficConfiguration interestData;
                bool shouldSkipLine = false;
                if (interestData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                  {
                    while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
                      {
                        lineNumber++;
                        if (!interestData.processConfigurationDetail(patternLine,
                                                                     m_logger, lineNumber))
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
                    if (interestData.checkTrafficDetailCorrectness())
                      m_trafficPatterns.push_back(interestData);
                  }
              }
          }
        patternFile.close();
        if (!checkTrafficPatternCorrectness())
          {
            m_logger.log("ERROR - Traffic Configuration Provided Is Not Proper- " +
                         m_configurationFile, false, true);
            m_logger.shutdownLogger();
            exit(EXIT_FAILURE);
          }
        m_logger.log("Traffic Configuration File Processing Completed\n", true, false);
        for (std::size_t patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
          {
            m_logger.log("Traffic Pattern Type #" +
                         std::to_string(patternId + 1), false, false);
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

  uint32_t
  getOldNonce()
  {
    if (m_nonces.size() == 0)
      return getNewNonce();
    int randomNonceIndex = std::rand() % m_nonces.size();
    return m_nonces[randomNonceIndex];
  }

  uint32_t
  getNewNonce()
  {
    //Performance Enhancement
    if (m_nonces.size() > 1000)
      m_nonces.clear();

    uint32_t randomNonce = static_cast<uint32_t>(std::rand());
    while (std::find(m_nonces.begin(), m_nonces.end(), randomNonce) != m_nonces.end())
      randomNonce = static_cast<uint32_t>(std::rand());

    m_nonces.push_back(randomNonce);
    return randomNonce;
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
  onData(const ndn::Interest& interest,
         ndn::Data& data,
         int globalReference,
         int localReference,
         int patternId,
         time::steady_clock::TimePoint sentTime)
  {
    std::string logLine = "Data Received      - PatternType=" + std::to_string(patternId + 1);
    logLine += ", GlobalID=" + std::to_string(globalReference);
    logLine += ", LocalID=" + std::to_string(localReference);
    logLine += ", Name=" + interest.getName().toUri();

    m_nInterestsReceived++;
    m_trafficPatterns[patternId].m_nInterestsReceived++;
    if (!m_trafficPatterns[patternId].m_expectedContent.empty())
      {
        std::string receivedContent = reinterpret_cast<const char*>(data.getContent().value());
        int receivedContentLength = data.getContent().value_size();
        receivedContent = receivedContent.substr(0, receivedContentLength);
        if (receivedContent != m_trafficPatterns[patternId].m_expectedContent)
          {
            m_nContentInconsistencies++;
            m_trafficPatterns[patternId].m_nContentInconsistencies++;
            logLine += ", IsConsistent=No";
          }
        else
          logLine += ", IsConsistent=Yes";
      }
    else
      logLine += ", IsConsistent=NotChecked";
    if (!m_hasQuietLogging)
      m_logger.log(logLine, true, false);
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
    if (m_nMaximumInterests >= 0 && globalReference == m_nMaximumInterests)
      {
        logStatistics();
        m_logger.shutdownLogger();
        m_face.shutdown();
        m_ioService.stop();
      }
  }

  void
  onTimeout(const ndn::Interest& interest,
            int globalReference,
            int localReference,
            int patternId)
  {
    std::string logLine = "Interest Timed Out - PatternType=" + std::to_string(patternId + 1);
    logLine += ", GlobalID=" + std::to_string(globalReference);
    logLine += ", LocalID=" + std::to_string(localReference);
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
        int trafficKey = std::rand() % 100;
        int cumulativePercentage = 0;
        std::size_t patternId;
        for (patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
          {
            cumulativePercentage += m_trafficPatterns[patternId].m_trafficPercentage;
            if (trafficKey <= cumulativePercentage)
              {
                Name interestName(m_trafficPatterns[patternId].m_name);
                if (m_trafficPatterns[patternId].m_nameAppendBytes > 0)
                  interestName.append(
                    getRandomByteString(m_trafficPatterns[patternId].m_nameAppendBytes));
                if (m_trafficPatterns[patternId].m_nameAppendSequenceNumber >= 0)
                  {
                    interestName.append(
                      std::to_string(m_trafficPatterns[patternId].m_nameAppendSequenceNumber));
                    m_trafficPatterns[patternId].m_nameAppendSequenceNumber++;
                  }

                Interest interest(interestName);
                if (m_trafficPatterns[patternId].m_minSuffixComponents >= 0)
                  interest.setMinSuffixComponents(
                    m_trafficPatterns[patternId].m_minSuffixComponents);
                if (m_trafficPatterns[patternId].m_maxSuffixComponents >= 0)
                  interest.setMaxSuffixComponents(
                    m_trafficPatterns[patternId].m_maxSuffixComponents);

                Exclude exclude;
                if (!m_trafficPatterns[patternId].m_excludeBefore.empty() &&
                    !m_trafficPatterns[patternId].m_excludeAfter.empty())
                  {
                    exclude.excludeRange(
                      name::Component(
                        m_trafficPatterns[patternId].m_excludeAfter),
                      name::Component(m_trafficPatterns[patternId].m_excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (!m_trafficPatterns[patternId].m_excludeBefore.empty())
                  {
                    exclude.excludeBefore(
                      name::Component(m_trafficPatterns[patternId].m_excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (!m_trafficPatterns[patternId].m_excludeAfter.empty())
                  {
                    exclude.excludeAfter(
                      name::Component(m_trafficPatterns[patternId].m_excludeAfter));
                    interest.setExclude(exclude);
                  }
                if (m_trafficPatterns[patternId].m_excludeBeforeBytes > 0 &&
                    m_trafficPatterns[patternId].m_excludeAfterBytes > 0)
                  {
                    exclude.excludeRange(
                      name::Component(
                        getRandomByteString(
                          m_trafficPatterns[patternId].m_excludeAfterBytes)),
                      name::Component(
                        getRandomByteString(
                          m_trafficPatterns[patternId].m_excludeBeforeBytes)));
                    interest.setExclude(exclude);
                  }
                else if (m_trafficPatterns[patternId].m_excludeBeforeBytes > 0)
                  {
                    exclude.excludeBefore(
                      name::Component(
                        getRandomByteString(
                          m_trafficPatterns[patternId].m_excludeBeforeBytes)));
                    interest.setExclude(exclude);
                  }
                else if (m_trafficPatterns[patternId].m_excludeAfterBytes > 0)
                  {
                    exclude.excludeAfter(
                      name::Component(
                        getRandomByteString(
                          m_trafficPatterns[patternId].m_excludeAfterBytes)));
                    interest.setExclude(exclude);
                  }

                if (m_trafficPatterns[patternId].m_childSelector >= 0)
                  interest.setChildSelector(m_trafficPatterns[patternId].m_childSelector);

                if (m_trafficPatterns[patternId].m_mustBeFresh == 0)
                  interest.setMustBeFresh(false);
                else if (m_trafficPatterns[patternId].m_mustBeFresh > 0)
                  interest.setMustBeFresh(true);
                if (m_trafficPatterns[patternId].m_nonceDuplicationPercentage > 0)
                  {
                    int duplicationPercentage = std::rand() % 100;
                    if (m_trafficPatterns[patternId].m_nonceDuplicationPercentage <=
                        duplicationPercentage)
                      interest.setNonce(getOldNonce());
                    else
                      interest.setNonce(getNewNonce());
                  }
                else
                  interest.setNonce(getNewNonce());
                if (m_trafficPatterns[patternId].m_interestLifetime >= time::milliseconds(0))
                  interest.setInterestLifetime(m_trafficPatterns[patternId].m_interestLifetime);

                try {
                  m_nInterestsSent++;
                  m_trafficPatterns[patternId].m_nInterestsSent++;
                  time::steady_clock::TimePoint sentTime = time::steady_clock::now();
                  m_face.expressInterest(interest,
                                         bind(&NdnTrafficClient::onData,
                                              this, _1, _2, m_nInterestsSent,
                                              m_trafficPatterns[patternId].m_nInterestsSent,
                                              patternId, sentTime),
                                         bind(&NdnTrafficClient::onTimeout,
                                              this, _1, m_nInterestsSent,
                                              m_trafficPatterns[patternId].m_nInterestsSent,
                                              patternId));

                  if (!m_hasQuietLogging) {
                    std::string logLine =
                      "Sending Interest   - PatternType=" + std::to_string(patternId + 1) +
                      ", GlobalID=" + std::to_string(m_nInterestsSent) +
                      ", LocalID=" +
                      std::to_string(m_trafficPatterns[patternId].m_nInterestsSent) +
                      ", Name=" + interest.getName().toUri();
                    m_logger.log(logLine, true, false);
                  }

                  timer->expires_at(timer->expires_at() +
                                    boost::posix_time::millisec(m_interestInterval.count()));
                  timer->async_wait(bind(&NdnTrafficClient::generateTraffic, this, timer));
                }
                catch (const std::exception& e) {
                  m_logger.log("ERROR: " + std::string(e.what()), true, true);
                }
                break;
              }
          }
        if (patternId == m_trafficPatterns.size())
          {
            timer->expires_at(timer->expires_at() +
                              boost::posix_time::millisec(m_interestInterval.count()));
            timer->async_wait(bind(&NdnTrafficClient::generateTraffic, this, timer));
          }
      }
  }

  void
  run()
  {
    boost::asio::signal_set signalSet(m_ioService, SIGINT, SIGTERM);
    signalSet.async_wait(bind(&NdnTrafficClient::signalHandler, this));
    m_logger.initializeLog(m_instanceId);
    initializeTrafficConfiguration();

    if (m_nMaximumInterests == 0)
      {
        logStatistics();
        m_logger.shutdownLogger();
        return;
      }

    boost::asio::deadline_timer deadlineTimer(m_ioService,
      boost::posix_time::millisec(m_interestInterval.count()));
    deadlineTimer.async_wait(bind(&NdnTrafficClient::generateTraffic, this, &deadlineTimer));

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
  time::milliseconds m_interestInterval;
  int m_nMaximumInterests;
  std::string m_configurationFile;
  boost::asio::io_service m_ioService;
  Face m_face;
  std::vector<InterestTrafficConfiguration> m_trafficPatterns;
  std::vector<uint32_t> m_nonces;
  int m_nInterestsSent;
  int m_nInterestsReceived;
  int m_nContentInconsistencies;

  //round trip time is stored as milliseconds with fractional
  //sub-milliseconds precision
  double m_minimumInterestRoundTripTime;
  double m_maximumInterestRoundTripTime;
  double m_totalInterestRoundTripTime;
};

} // namespace ndn

int
main(int argc, char* argv[])
{
  std::srand(std::time(nullptr));

  ndn::NdnTrafficClient client(argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hqi:c:")) != -1) {
    switch (option) {
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
