/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2014 University of Arizona.
 *
 * GNU 3.0 License, see the LICENSE file for more information
 *
 * Author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include <sstream>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include "logger.hpp"

namespace ndn {

class NdnTrafficServer : boost::noncopyable
{
public:

  explicit
  NdnTrafficServer(char* programName)
    : m_logger("NdnTrafficServer")
    , m_programName(programName)
    , m_hasError(false)
    , m_hasQuietLogging(false)
    , m_nRegistrationsFailed(0)
    , m_nMaximumInterests(-1)
    , m_nInterestsReceived(0)
    , m_contentDelay(time::milliseconds(-1))
    , m_face(m_ioService)
  {
    m_instanceId = boost::lexical_cast<std::string>(std::rand());
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
    {
    }

    void
    printTrafficConfiguration(Logger& logger)
    {
      std::string detail = "";
      if (m_name != "")
        detail += "Name=" + m_name + ", ";
      if (m_contentType >= 0)
        detail += "ContentType=" + boost::lexical_cast<std::string>(m_contentType) + ", ";
      if (m_freshnessPeriod >= time::milliseconds(0))
        detail += "FreshnessPeriod=" +
          boost::lexical_cast<std::string>(static_cast<int>(m_freshnessPeriod.count())) + ", ";
      if (m_contentBytes >= 0)
        detail += "ContentBytes=" + boost::lexical_cast<std::string>(m_contentBytes) + ", ";
      if (m_contentDelay >= time::milliseconds(0))
        detail += "ContentDelay=" +
          boost::lexical_cast<std::string>(m_contentDelay.count()) + ", ";
      if (m_content != "")
        detail += "Content=" + m_content + ", ";
      if (detail.length() >= 2)
        detail = detail.substr(0, detail.length() - 2);
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
      if(parameter == "" || value == "")
        return false;
      return true;
    }

    bool
    processConfigurationDetail(const std::string& detail,
                               Logger& logger,
                               int lineNumber)
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value))
        {
          if (parameter == "Name")
            m_name = value;
          else if (parameter == "ContentType")
            m_contentType = boost::lexical_cast<int>(value);
          else if (parameter == "FreshnessPeriod")
            m_freshnessPeriod = time::milliseconds(boost::lexical_cast<int>(value));
          else if (parameter == "ContentDelay")
            m_contentDelay = time::milliseconds(boost::lexical_cast<int>(value));
          else if (parameter == "ContentBytes")
            m_contentBytes = boost::lexical_cast<int>(value);
          else if (parameter == "Content")
            m_content = value;
          else
            logger.log("Line " + boost::lexical_cast<std::string>(lineNumber) +
                       " \t- Invalid Parameter='" + parameter + "'", false, true);
        }
      else
        {
          logger.log("Line " + boost::lexical_cast<std::string>(lineNumber) +
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

    std::string m_name;
    int m_contentType;
    time::milliseconds m_freshnessPeriod;
    int m_contentBytes;
    time::milliseconds m_contentDelay;
    std::string m_content;
    int m_nInterestsReceived;

  };

  void
  usage()
  {

    std::cout << "\nUsage: " << m_programName << " [options] <Traffic_Configuration_File>\n"
      "Respond to Interest as per provided Traffic Configuration File\n"
      "Multiple Prefixes can be configured for handling.\n"
      "Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.\n"
      "  [-d interval] - set delay before responding to interest in milliseconds\n"
      "  [-c count]    - specify maximum number of interests to be satisfied\n"
      "  [-q]          - quiet logging - no interest reception/data generation messages\n"
      "  [-h]          - print help and exit\n\n";
    exit(1);

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
      boost::lexical_cast<std::string>(m_trafficPatterns.size()), false, true);
    m_logger.log("Total Interests Received    = " +
      boost::lexical_cast<std::string>(m_nInterestsReceived), false, true);
    if (m_nInterestsReceived < m_nMaximumInterests)
      m_hasError = true;
    for (int patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      {
        m_logger.log("\nTraffic Pattern Type #" +
          boost::lexical_cast<std::string>(patternId + 1), false, true);
        m_trafficPatterns[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Received    = " +
          boost::lexical_cast<std::string>(
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
        int patternId = 0;
        int lineNumber = 0;
        while (getline(patternFile, patternLine))
          {
            lineNumber++;
            if (std::isalpha(patternLine[0]))
              {
                DataTrafficConfiguration dataData;
                bool shouldSkipLine = false;
                patternId++;
                if (dataData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                  {
                    while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
                      {
                        lineNumber++;
                        if (!dataData.processConfigurationDetail(patternLine,
                                                                 m_logger,
                                                                 lineNumber))
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

  static std::string
  getRandomByteString(int randomSize)
  {
    std::string randomString;
    for (int i = 0; i < randomSize; i++)
      randomString += static_cast<char>(std::rand() % 128);
    return randomString;
  }

  void
  onInterest(const Name& name, const Interest& interest, int patternId)
  {
    if (m_nMaximumInterests < 0 || m_nInterestsReceived < m_nMaximumInterests)
      {
        Data data(interest.getName());

        if (m_trafficPatterns[patternId].m_contentType >= 0)
          data.setContentType(m_trafficPatterns[patternId].m_contentType);

        if (m_trafficPatterns[patternId].m_freshnessPeriod >= time::milliseconds(0))
          data.setFreshnessPeriod(m_trafficPatterns[patternId].m_freshnessPeriod);

        std::string content = "";
        if (m_trafficPatterns[patternId].m_contentBytes >= 0)
          content = getRandomByteString(m_trafficPatterns[patternId].m_contentBytes);
        if (m_trafficPatterns[patternId].m_content != "")
          content = m_trafficPatterns[patternId].m_content;

        data.setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
        m_keyChain.sign(data);
        m_nInterestsReceived++;
        m_trafficPatterns[patternId].m_nInterestsReceived++;
        std::string logLine = "Interest Received          - PatternType=" +
          boost::lexical_cast<std::string>(patternId + 1);
        logLine += ", GlobalID=" + boost::lexical_cast<std::string>(m_nInterestsReceived);
        logLine += ", LocalID=" +
          boost::lexical_cast<std::string>(m_trafficPatterns[patternId].m_nInterestsReceived);
        logLine += ", Name=" + m_trafficPatterns[patternId].m_name;
        if (!m_hasQuietLogging)
          m_logger.log(logLine, true, false);
        if (m_trafficPatterns[patternId].m_contentDelay > time::milliseconds(-1))
          usleep(m_trafficPatterns[patternId].m_contentDelay.count() * 1000);
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
    std::string logLine = "";
    logLine += "Prefix Registration Failed - PatternType=" +
      boost::lexical_cast<std::string>(patternId + 1);
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

    for (int patternId = 0; patternId < m_trafficPatterns.size(); patternId++)
      {
        m_face.setInterestFilter(m_trafficPatterns[patternId].m_name,
                                 bind(&NdnTrafficServer::onInterest,
                                      this, _1, _2,
                                      patternId),
                                 bind(&NdnTrafficServer::onRegisterFailed,
                                      this, _1, _2,
                                      patternId));
      }

    try {
      m_face.processEvents();
    }
    catch (std::exception& e) {
      m_logger.log("ERROR: " + static_cast<std::string>(e.what()), true, true);
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
  int m_nRegistrationsFailed;
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
  std::srand(std::time(0));
  ndn::NdnTrafficServer ndnTrafficServer(argv[0]);
  int option;
  while ((option = getopt(argc, argv, "hqc:d:")) != -1) {
    switch (option) {
    case 'h':
      ndnTrafficServer.usage();
      break;
    case 'c':
      ndnTrafficServer.setMaximumInterests(atoi(optarg));
      break;
    case 'd':
      ndnTrafficServer.setContentDelay(atoi(optarg));
      break;
    case 'q':
      ndnTrafficServer.setQuietLogging();
      break;
    default:
      ndnTrafficServer.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == 0)
    ndnTrafficServer.usage();

  ndnTrafficServer.setConfigurationFile(argv[0]);
  ndnTrafficServer.run();

  if (ndnTrafficServer.hasError())
    return 1;
  else
    return 0;
}
