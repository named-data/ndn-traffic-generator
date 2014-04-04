/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2014 University of Arizona.
 *
 * GNU 3.0 License, see the LICENSE file for more information
 *
 * Author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include <string>
#include <sstream>
#include <fstream>
#include <vector>

#include "logger.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <ndn-cpp-dev/face.hpp>
#include <ndn-cpp-dev/exclude.hpp>
#include <ndn-cpp-dev/name-component.hpp>

namespace ndn {

class NdnTrafficClient
{
public:

  explicit
  NdnTrafficClient(char* programName)
    : m_programName(programName)
    , m_logger("NdnTrafficClient")
    , m_hasError(false)
    , m_ioService(new boost::asio::io_service)
    , m_face(m_ioService)
    , m_interestInterval(getDefaultInterestInterval())
    , m_nMaximumInterests(-1)
    , m_nInterestsSent(0)
    , m_nInterestsReceived(0)
    , m_nContentInconsistencies(0)
    , m_minimumInterestRoundTripTime(std::numeric_limits<double>::max())
    , m_maximumInterestRoundTripTime(0)
    , m_totalInterestRoundTripTime(0)
  {
    m_instanceId = boost::lexical_cast<std::string>(std::rand());
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
      , m_scope(-1)
      , m_interestLifetime(getDefaultInterestLifetime())
      , m_nInterestsSent(0)
      , m_nInterestsReceived(0)
      , m_minimumInterestRoundTripTime(std::numeric_limits<double>::max())
      , m_maximumInterestRoundTripTime(0)
      , m_totalInterestRoundTripTime(0)
      , m_nContentInconsistencies(0)
    {
    }

    time::milliseconds
    getDefaultInterestLifetime()
    {
      return time::milliseconds(-1);
    }

    void
    printTrafficConfiguration(Logger& logger)
    {
      std::string detail = "";
      if (m_trafficPercentage > 0)
        detail += "TrafficPercentage=" +
          boost::lexical_cast<std::string>(m_trafficPercentage) + ", ";
      if (m_name != "")
        detail += "Name=" + m_name + ", ";
      if (m_nameAppendBytes > 0)
        detail += "NameAppendBytes=" + boost::lexical_cast<std::string>(m_nameAppendBytes) + ", ";
      if (m_nameAppendSequenceNumber > 0)
        detail += "NameAppendSequenceNumber=" +
          boost::lexical_cast<std::string>(m_nameAppendSequenceNumber) + ", ";
      if (m_minSuffixComponents >= 0)
        detail += "MinSuffixComponents=" +
          boost::lexical_cast<std::string>(m_minSuffixComponents) + ", ";
      if (m_maxSuffixComponents >= 0)
        detail += "MaxSuffixComponents=" +
          boost::lexical_cast<std::string>(m_maxSuffixComponents) + ", ";
      if (m_excludeBefore != "")
        detail += "ExcludeBefore=" + m_excludeBefore + ", ";
      if (m_excludeAfter != "")
        detail += "ExcludeAfter=" + m_excludeAfter + ", ";
      if (m_excludeBeforeBytes > 0)
        detail += "ExcludeBeforeBytes=" +
          boost::lexical_cast<std::string>(m_excludeBeforeBytes) + ", ";
      if (m_excludeAfterBytes > 0)
        detail += "ExcludeAfterBytes=" +
          boost::lexical_cast<std::string>(m_excludeAfterBytes) + ", ";
      if (m_childSelector >= 0)
        detail += "ChildSelector=" +
          boost::lexical_cast<std::string>(m_childSelector) + ", ";
      if (m_mustBeFresh >= 0)
        detail += "MustBeFresh=" +
          boost::lexical_cast<std::string>(m_mustBeFresh) + ", ";
      if (m_nonceDuplicationPercentage > 0)
        detail += "NonceDuplicationPercentage=" +
          boost::lexical_cast<std::string>(m_nonceDuplicationPercentage) + ", ";
      if (m_scope >= 0)
        detail += "Scope="+boost::lexical_cast<std::string>(m_scope) + ", ";
      if (m_interestLifetime >= time::milliseconds(0))
        detail += "InterestLifetime=" +
          boost::lexical_cast<std::string>(m_interestLifetime.count()) + ", ";
      if (m_expectedContent != "")
        detail += "ExpectedContent=" + m_expectedContent + ", ";
      if (detail.length() >= 2)
        detail = detail.substr(0, detail.length() - 2); //Removing suffix ", "
      logger.log(detail, false, false);
    }

    bool
    extractParameterValue(const std::string& detail,
                          std::string& parameter,
                          std::string& value)
    {
      std::string allowedCharacters = ":/+._-%";
      parameter = "";
      value = "";
      int i = 0;
      while (detail[i] != '=' && i < detail.length())
        {
          parameter += detail[i];
          i++;
        }
      if (i == detail.length())
        return false;
      i++;
      while ((std::isalnum(detail[i]) ||
             allowedCharacters.find(detail[i]) != std::string::npos) &&
             i < detail.length())
        {
          value += detail[i];
          i++;
        }
      if (parameter == "" || value == "")
        return false;
      return true;
    }

    bool
    processConfigurationDetail(const std::string& detail, Logger& logger, int lineNumber)
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value))
        {
          if (parameter == "TrafficPercentage")
            m_trafficPercentage = boost::lexical_cast<int>(value);
          else if (parameter == "Name")
            m_name = value;
          else if (parameter == "NameAppendBytes")
            m_nameAppendBytes = boost::lexical_cast<int>(value);
          else if (parameter == "NameAppendSequenceNumber")
            m_nameAppendSequenceNumber = boost::lexical_cast<int>(value);
          else if (parameter == "MinSuffixComponents")
            m_minSuffixComponents = boost::lexical_cast<int>(value);
          else if (parameter == "MaxSuffixComponents")
            m_maxSuffixComponents = boost::lexical_cast<int>(value);
          else if (parameter == "ExcludeBefore")
            m_excludeBefore = value;
          else if (parameter == "ExcludeAfter")
            m_excludeAfter = value;
          else if (parameter == "ExcludeBeforeBytes")
            m_excludeBeforeBytes = boost::lexical_cast<int>(value);
          else if (parameter == "ExcludeAfterBytes")
            m_excludeAfterBytes = boost::lexical_cast<int>(value);
          else if (parameter == "ChildSelector")
            m_childSelector = boost::lexical_cast<int>(value);
          else if (parameter == "MustBeFresh")
            m_mustBeFresh = boost::lexical_cast<int>(value);
          else if (parameter == "NonceDuplicationPercentage")
            m_nonceDuplicationPercentage = boost::lexical_cast<int>(value);
          else if (parameter == "Scope")
            m_scope = boost::lexical_cast<int>(value);
          else if (parameter == "InterestLifetime")
            m_interestLifetime = time::milliseconds(boost::lexical_cast<int>(value));
          else if (parameter == "ExpectedContent")
            m_expectedContent = value;
          else
            logger.log("Line " + boost::lexical_cast<std::string>(lineNumber) +
                       " \t- Invalid Parameter='" + parameter + "'", false, true);
        }
      else
        {
          logger.log("Line " + boost::lexical_cast<std::string>(lineNumber) +
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
    int m_scope;
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
  }; // class InterestTrafficConfiguration

  bool
  hasError() const
  {
    return m_hasError;
  }

  void
  usage()
  {
    std::cout << "\nUsage: " << m_programName << " [options] <Traffic_Configuration_File>\n"
      "Generate Interest Traffic as per provided Traffic Configuration File\n"
      "Interests are continuously generated unless a total number is specified.\n"
      "Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.\n"
      "  [-i interval] - set interest generation interval in milliseconds (default "
              << getDefaultInterestInterval() << ")\n"
      "  [-c count]    - set total number of interests to be generated\n"
      "  [-h]          - print help and exit\n\n";
    exit(1);
  }

  time::milliseconds
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
  signalHandler()
  {
    logStatistics();
    m_logger.shutdownLogger();
    m_face.shutdown();
    m_ioService->stop();
    if (m_hasError)
      exit(1);
    else
      exit(0);
  }

  void
  logStatistics()
  {
    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Traffic Pattern Types = " +
      boost::lexical_cast<std::string>(static_cast<int>(m_trafficPatterns.size())), false, true);
    m_logger.log("Total Interests Sent        = " +
      boost::lexical_cast<std::string>(m_nInterestsSent), false, true);
    m_logger.log("Total Responses Received    = " +
      boost::lexical_cast<std::string>(m_nInterestsReceived), false, true);
    double loss = 0;
    if (m_nInterestsSent > 0)
      loss = (m_nInterestsSent - m_nInterestsReceived) * 100.0 / m_nInterestsSent;
    m_logger.log("Total Interest Loss         = " +
      boost::lexical_cast<std::string>(loss) + "%", false, true);
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
      boost::lexical_cast<std::string>(inconsistency) + "%", false, true);
    m_logger.log("Total Round Trip Time       = " +
      boost::lexical_cast<std::string>(m_totalInterestRoundTripTime) + "ms", false, true);
    m_logger.log("Average Round Trip Time     = " +
      boost::lexical_cast<std::string>(average) + "ms\n", false, true);

    for (int patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      {
        m_logger.log("Traffic Pattern Type #" +
          boost::lexical_cast<std::string>(patternId + 1), false, true);
        m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Sent        = " +
          boost::lexical_cast<std::string>(
            m_trafficPatterns[patternId].m_nInterestsSent), false, true);
        m_logger.log("Total Responses Received    = " +
          boost::lexical_cast<std::string>(
            m_trafficPatterns[patternId].m_nInterestsReceived), false, true);
        loss = 0;
        if (m_trafficPatterns[patternId].m_nInterestsSent > 0)
          {
            loss = (m_trafficPatterns[patternId].m_nInterestsSent -
                    m_trafficPatterns[patternId].m_nInterestsReceived);
            loss *= 100.0;
            loss /= m_trafficPatterns[patternId].m_nInterestsSent;
          }
        m_logger.log("Total Interest Loss         = " +
          boost::lexical_cast<std::string>(loss) + "%", false, true);
        average = 0;
        inconsistency = 0;
        if (m_trafficPatterns[patternId].m_nInterestsReceived > 0)
          {
            average = (m_trafficPatterns[patternId].m_totalInterestRoundTripTime /
                       m_trafficPatterns[patternId].m_nInterestsReceived);
            inconsistency = m_trafficPatterns[patternId].m_nContentInconsistencies;
            inconsistency =
              inconsistency * 100.0 / m_trafficPatterns[patternId].m_nInterestsReceived;
          }
        m_logger.log("Total Data Inconsistency    = " +
          boost::lexical_cast<std::string>(inconsistency) + "%", false, true);
        m_logger.log("Total Round Trip Time       = " +
          boost::lexical_cast<std::string>(
            m_trafficPatterns[patternId].m_totalInterestRoundTripTime) + "ms", false, true);
        m_logger.log("Average Round Trip Time     = " +
          boost::lexical_cast<std::string>(average) + "ms\n", false, true);
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
        int patternId = 0;
        int lineNumber = 0;
        while (getline(patternFile, patternLine))
          {
            lineNumber++;
            if (std::isalpha(patternLine[0]))
              {
                InterestTrafficConfiguration interestData;
                bool shouldSkipLine = false;
                patternId++;
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
            exit(1);
          }
        m_logger.log("Traffic Configuration File Processing Completed\n", true, false);
        for (patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
          {
            m_logger.log("Traffic Pattern Type #" +
              boost::lexical_cast<std::string>(patternId + 1), false, false);
            m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
            m_logger.log("", false, false);
          }
      }
    else
      {
        m_logger.log("ERROR - Unable To Open Traffic Configuration File: " +
                     m_configurationFile, false, true);
        m_logger.shutdownLogger();
        exit(1);
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
            exit(1);
          }
      }
    else
      {
        m_logger.log("ERROR - Traffic Configuration File Does Not Exist: " +
                     m_configurationFile, false, true);
        m_logger.shutdownLogger();
        exit(1);
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
  getRandomByteString(int randomSize)
  {
    std::string randomString;
    for (int i = 0; i < randomSize; i++)
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
    std::string logLine =
      "Data Received      - PatternType=" + boost::lexical_cast<std::string>(patternId+1);
    logLine += ", GlobalID=" + boost::lexical_cast<std::string>(globalReference);
    logLine += ", LocalID=" + boost::lexical_cast<std::string>(localReference);
    logLine += ", Name=" + interest.getName().toUri();

    m_nInterestsReceived++;
    m_trafficPatterns[patternId].m_nInterestsReceived++;
    if (m_trafficPatterns[patternId].m_expectedContent != "")
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
        m_ioService->stop();
      }
  }

  void
  onTimeout(const ndn::Interest& interest,
            int globalReference,
            int localReference,
            int patternId)
  {
    std::string logLine = "Interest Timed Out - PatternType=" +
      boost::lexical_cast<std::string>(patternId + 1);
    logLine += ", GlobalID=" + boost::lexical_cast<std::string>(globalReference);
    logLine += ", LocalID=" + boost::lexical_cast<std::string>(localReference);
    logLine += ", Name=" + interest.getName().toUri();
    m_logger.log(logLine, true, false);
    if (m_nMaximumInterests >= 0 && globalReference == m_nMaximumInterests)
      {
        logStatistics();
        m_logger.shutdownLogger();
        m_face.shutdown();
        m_ioService->stop();
      }
  }

  void
  generateTraffic(boost::asio::deadline_timer* deadlineTimer)
  {
    if (m_nMaximumInterests < 0 || m_nInterestsSent < m_nMaximumInterests)
      {
        int trafficKey = std::rand() % 100;
        int cumulativePercentage = 0;
        int patternId;
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
                      boost::lexical_cast<std::string>(
                        m_trafficPatterns[patternId].m_nameAppendSequenceNumber));
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
                if (m_trafficPatterns[patternId].m_excludeBefore != "" &&
                    m_trafficPatterns[patternId].m_excludeAfter != "")
                  {
                    exclude.excludeRange(
                      name::Component(
                        m_trafficPatterns[patternId].m_excludeAfter),
                      name::Component(m_trafficPatterns[patternId].m_excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (m_trafficPatterns[patternId].m_excludeBefore != "")
                  {
                    exclude.excludeBefore(
                      name::Component(m_trafficPatterns[patternId].m_excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (m_trafficPatterns[patternId].m_excludeAfter != "")
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
                if (m_trafficPatterns[patternId].m_scope >= 0)
                  interest.setScope(m_trafficPatterns[patternId].m_scope);
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
                  std::string logLine = "";
                  logLine += "Sending Interest   - PatternType=" +
                    boost::lexical_cast<std::string>(patternId+1);
                  logLine += ", GlobalID=" + boost::lexical_cast<std::string>(m_nInterestsSent);
                  logLine += ", LocalID=" + boost::lexical_cast<std::string>(
                    m_trafficPatterns[patternId].m_nInterestsSent);
                  logLine += ", Name="+interest.getName().toUri();
                  m_logger.log(logLine, true, false);
                  deadlineTimer->expires_at(deadlineTimer->expires_at() +
                                            boost::posix_time::millisec(
                                              m_interestInterval.count()));
                  deadlineTimer->async_wait(bind(&NdnTrafficClient::generateTraffic,
                                                 this, deadlineTimer));
                }
                catch (std::exception& e) {
                  m_logger.log("ERROR: " + static_cast<std::string>(e.what()), true, true);
                }
                break;
              }
          }
        if (patternId == m_trafficPatterns.size())
          {
            deadlineTimer->expires_at(deadlineTimer->expires_at() +
                                      boost::posix_time::millisec(
                                        m_interestInterval.count()));
            deadlineTimer->async_wait(bind(&NdnTrafficClient::generateTraffic,
                                           this, deadlineTimer));
          }
      }
  }

  void
  run()
  {
    boost::asio::signal_set signalSet(*m_ioService, SIGINT, SIGTERM);
    signalSet.async_wait(bind(&NdnTrafficClient::signalHandler, this));
    m_logger.initializeLog(m_instanceId);
    initializeTrafficConfiguration();

    if (m_nMaximumInterests == 0)
      {
        logStatistics();
        m_logger.shutdownLogger();
        return;
      }

    boost::asio::deadline_timer deadlineTimer(
      *m_ioService,
      boost::posix_time::millisec(m_interestInterval.count()));
    deadlineTimer.async_wait(bind(&NdnTrafficClient::generateTraffic,
                                  this, &deadlineTimer));
    try {
      m_face.processEvents();
    }
    catch(std::exception& e) {
      m_logger.log("ERROR: " + static_cast<std::string>(e.what()), true, true);
      m_logger.shutdownLogger();
      m_hasError = true;
      m_ioService->stop();
    }
  }

private:

  std::string m_programName;
  std::string m_instanceId;
  bool m_hasError;
  time::milliseconds m_interestInterval;
  int m_nMaximumInterests;
  Logger m_logger;
  std::string m_configurationFile;
  shared_ptr<boost::asio::io_service> m_ioService;
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
  std::srand(std::time(0));
  ndn::NdnTrafficClient ndnTrafficClient (argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hi:c:")) != -1) {
    switch (option) {
    case 'h':
      ndnTrafficClient.usage();
      break;
    case 'i':
      ndnTrafficClient.setInterestInterval(atoi(optarg));
      break;
    case 'c':
      ndnTrafficClient.setMaximumInterests(atoi(optarg));
      break;
    default:
      ndnTrafficClient.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == 0)
    ndnTrafficClient.usage();

  ndnTrafficClient.setConfigurationFile(argv[0]);
  ndnTrafficClient.run();

  if (ndnTrafficClient.hasError())
    return 1;
  else
    return 0;
}
