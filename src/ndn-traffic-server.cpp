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

#include <ndn-cpp-dev/face.hpp>
#include <ndn-cpp-dev/security/key-chain.hpp>

#include "logger.hpp"

namespace ndn {

class NdnTrafficServer
{
public:
  NdnTrafficServer(char* programName)
    : m_logger("NDNTrafficServer")
    , ioService_(new boost::asio::io_service)
    , face_(ioService_)
    , keyChain_()
  {
    std::srand(std::time(0));
    instanceId_ = toString(std::rand());
    programName_ = programName;
    contentDelayTime_ = getDefaultContentDelayTime();
    totalRegistrationsFailed_ = 0;
    configurationFile_ = "";
    totalInterestReceived_ = 0;
  }

  class DataTrafficConfiguration
  {
  public:
    DataTrafficConfiguration()
    {
      name = "";
      contentType = -1;
      freshnessPeriod = -1;
      contentBytes = -1;
      content = "";
      totalInterestReceived = 0;
    }

    void
    printTrafficConfiguration(Logger& logger)
    {
      std::string detail;
      detail = "";
      if (name != "")
        detail += "Name="+name+", ";
      if (contentType >= 0)
        detail += "ContentType="+toString(contentType)+", ";
      if (freshnessPeriod >= 0)
        detail += "FreshnessPeriod="+toString(freshnessPeriod)+", ";
      if (contentBytes >= 0)
        detail += "ContentBytes="+toString(contentBytes)+", ";
      if (content != "")
        detail += "Content="+content+", ";
      if (detail.length() >= 2)
        detail = detail.substr(0, detail.length()-2);
      logger.log(detail, false, false);
    }


    bool
    extractParameterValue(const std::string& detail,
                          std::string& parameter, std::string& value)
    {
      int i;
      std::string allowedCharacters = ":/+._-%";
      parameter = "";
      value = "";
      i = 0;
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
                               Logger& logger, int lineNumber)
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value))
        {
          if (parameter == "Name")
            name = value;
          else if (parameter == "ContentType")
            contentType = toInteger(value);
          else if (parameter == "FreshnessPeriod")
            freshnessPeriod = toInteger(value);
          else if (parameter == "ContentBytes")
            contentBytes = toInteger(value);
          else if (parameter == "Content")
            content = value;
          else
            logger.log("Line "+toString(lineNumber)+
                       " \t- Invalid Parameter='"+parameter+"'", false, true);
        }
      else
        {
          logger.log("Line "+toString(lineNumber)+
                     " \t- Improper Traffic Configuration Line- "+detail, false, true);
          return false;
        }
      return true;
    }

    bool
    checkTrafficDetailCorrectness()
    {
      return true;
    }

    std::string name;
    int contentType;
    int freshnessPeriod;
    int contentBytes;
    std::string content;
    int totalInterestReceived;

  };

  std::string
  getDefaultContent()
  {
    return "";
  }

  static std::string
  toString(int integerValue)
  {
    std::stringstream stream;
    stream << integerValue;
    return stream.str();
  }

  static int
  toInteger(std::string stringValue)
  {
    int integerValue;
    std::stringstream stream(stringValue);
    stream >> integerValue;
    return integerValue;
  }

  void
  usage()
  {

    std::cout << "\nUsage: " << programName_ << " [options] <Traffic_Configuration_File>\n"
      "Respond to Interest as per provided Traffic Configuration File\n"
      "Multiple Prefixes can be configured for handling.\n"
      "Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.\n"
      "  [-d interval] - set delay before responding to interest in milliseconds (minimum "
              << getDefaultContentDelayTime() << " milliseconds)\n"
      "  [-h] - print help and exit\n\n";
    exit(1);

  }

  int
  getDefaultContentDelayTime()
  {
    return 0;
  }

  void
  setContentDelayTime(int contentDelayTime)
  {
    if (contentDelayTime < 0)
      usage();
    contentDelayTime_ = contentDelayTime;
  }

  void
  setConfigurationFile(char* configurationFile)
  {
    configurationFile_ = configurationFile;
  }

  void
  signalHandler()
  {
    m_logger.shutdownLogger();
    face_.shutdown();
    ioService_.reset();
    logStatistics();
    exit(1);
  }

  void
  logStatistics()
  {
    int patternId;
    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Traffic Pattern Types = " +
                 toString((int)trafficPattern_.size()), false, true);
    m_logger.log("Total Interests Received    = " +
                 toString(totalInterestReceived_), false, true);

    for (patternId=0; patternId<trafficPattern_.size(); patternId++)
      {
        m_logger.log("\nTraffic Pattern Type #"+toString(patternId+1), false, true);
        trafficPattern_[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Received    = " +
                     toString(trafficPattern_[patternId].totalInterestReceived)+"\n", false, true);
      }
  }

  bool
  checkTrafficPatternCorrectness()
  {
    return true;
  }

  void
  analyzeConfigurationFile()
  {
    int patternId;
    int lineNumber;
    bool skipLine;
    std::string patternLine;
    std::ifstream patternFile;
    m_logger.log("Analyzing Traffic Configuration File: " + configurationFile_, true, true);
    patternFile.open(configurationFile_.c_str());
    if (patternFile.is_open())
      {
        patternId = 0;
        lineNumber = 0;
        while (getline(patternFile, patternLine))
          {
            lineNumber++;
            if (std::isalpha(patternLine[0]))
              {
                DataTrafficConfiguration dataData;
                skipLine = false;
                patternId++;
                if (dataData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                  {
                    while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
                      {
                        lineNumber++;
                        if (!dataData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                          {
                            skipLine = true;
                            break;
                          }
                      }
                    lineNumber++;
                  }
                else
                  skipLine = true;
                if( !skipLine )
                  {
                    if (dataData.checkTrafficDetailCorrectness())
                      trafficPattern_.push_back(dataData);
                  }
              }
          }
        patternFile.close();
        if (!checkTrafficPatternCorrectness())
          {
            m_logger.log("ERROR - Traffic Configuration Provided Is Not Proper- " +
                         configurationFile_, false, true);
            m_logger.shutdownLogger();
            exit(1);
          }
        m_logger.log("Traffic Configuration File Processing Completed\n", true, false);
        for (patternId = 0; patternId < trafficPattern_.size(); patternId++)
          {
            m_logger.log("Traffic Pattern Type #"+toString(patternId+1), false, false);
            trafficPattern_[patternId].printTrafficConfiguration(m_logger);
            m_logger.log("", false, false);
          }
      }
    else
      {
        m_logger.log("ERROR - Unable To Open Traffic Configuration File: " +
                     configurationFile_, false, true);
        m_logger.shutdownLogger();
        exit(1);
      }
  }

  void
  initializeTrafficConfiguration()
  {
    if (boost::filesystem::exists(boost::filesystem::path(configurationFile_)))
      {
        if(boost::filesystem::is_regular_file(boost::filesystem::path(configurationFile_)))
          {
            analyzeConfigurationFile();
          }
        else
          {
            m_logger.log("ERROR - Traffic Configuration File Is Not A Regular File: " +
                         configurationFile_, false, true);
            m_logger.shutdownLogger();
            exit(1);
          }
      }
    else
      {
        m_logger.log("ERROR - Traffic Configuration File Does Not Exist: " +
                     configurationFile_, false, true);
        m_logger.shutdownLogger();
        exit(1);
      }
  }

  static std::string
  getRandomByteString(int randomSize)
  {
    int i;
    std::string characterSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw0123456789";
    std::string randomData;
    for (i=0; i<randomSize; i++)
      randomData += characterSet[std::rand() % characterSet.length()];
    return randomData;
  }

  void
  onInterest(const Name& name, const Interest& interest, int patternId)
  {
    std::string content, logLine;
    content = "";
    logLine = "";

    Data data(interest.getName());
    if (trafficPattern_[patternId].contentType >= 0)
      data.setContentType(trafficPattern_[patternId].contentType);
    if (trafficPattern_[patternId].freshnessPeriod >= 0)
      data.setFreshnessPeriod(time::milliseconds(trafficPattern_[patternId].freshnessPeriod));
    if (trafficPattern_[patternId].contentBytes >= 0)
      content = getRandomByteString(trafficPattern_[patternId].contentBytes);
    if (trafficPattern_[patternId].content != "")
      content = trafficPattern_[patternId].content;
    data.setContent((const uint8_t*)content.c_str(), content.length());
    keyChain_.sign(data);
    totalInterestReceived_++;
    trafficPattern_[patternId].totalInterestReceived++;
    logLine += "Interest Received          - PatternType="+toString(patternId+1);
    logLine += ", GlobalID="+toString(totalInterestReceived_);
    logLine += ", LocalID="+toString(trafficPattern_[patternId].totalInterestReceived);
    logLine += ", Name="+trafficPattern_[patternId].name;
    m_logger.log(logLine, true, false);
    usleep(contentDelayTime_*1000);
    face_.put(data);
  }

  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason, int patternId)
  {
    std::string logLine;
    logLine = "";
    logLine += "Prefix Registration Failed - PatternType="+toString(patternId+1);
    logLine += ", Name="+trafficPattern_[patternId].name;
    m_logger.log(logLine, true, true);
    totalRegistrationsFailed_++;
    if (totalRegistrationsFailed_ == trafficPattern_.size())
      signalHandler();
  }

  void
  initialize()
  {
    boost::asio::signal_set signalSet(*ioService_, SIGINT, SIGTERM);
    signalSet.async_wait(boost::bind(&NdnTrafficServer::signalHandler, this));
    m_logger.initializeLog(instanceId_);
    initializeTrafficConfiguration();

    int patternId;
    for (patternId=0; patternId<trafficPattern_.size(); patternId++)
      {
        face_.setInterestFilter(trafficPattern_[patternId].name,
                                bind(&NdnTrafficServer::onInterest,
                                     this, _1, _2,
                                     patternId),
                                bind(&NdnTrafficServer::onRegisterFailed,
                                     this, _1, _2,
                                     patternId));
      }

    try {
      face_.processEvents();
    }
    catch(std::exception &e) {
      m_logger.log("ERROR: "+(std::string)e.what(), true, true);
      m_logger.shutdownLogger();
    }
  }

private:
  KeyChain keyChain_;
  std::string programName_;
  std::string instanceId_;
  int contentDelayTime_;
  int totalRegistrationsFailed_;
  Logger m_logger;
  std::string configurationFile_;
  ptr_lib::shared_ptr<boost::asio::io_service> ioService_;
  Face face_;
  std::vector<DataTrafficConfiguration> trafficPattern_;
  int totalInterestReceived_;
};

} // namespace ndn

int
main(int argc, char* argv[])
{
  int option;
  ndn::NdnTrafficServer ndnTrafficServer (argv[0]);
  while ((option = getopt(argc, argv, "hd:")) != -1) {
    switch (option) {
    case 'h'  :
      ndnTrafficServer.usage();
      break;
    case 'd'  :
      ndnTrafficServer.setContentDelayTime(atoi(optarg));
      break;
    default   :
      ndnTrafficServer.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == NULL)
    ndnTrafficServer.usage();

  ndnTrafficServer.setConfigurationFile(argv[0]);
  ndnTrafficServer.initialize();

  return 0;
}
