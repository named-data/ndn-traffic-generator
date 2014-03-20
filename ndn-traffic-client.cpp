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
#include <boost/date_time/posix_time/posix_time.hpp>

#include <ndn-cpp-dev/face.hpp>
#include <ndn-cpp-dev/exclude.hpp>
#include <ndn-cpp-dev/name-component.hpp>
#include <ndn-cpp-dev/security/key-chain.hpp>

namespace ndn {

class NdnTrafficClient
{
public:

  NdnTrafficClient( char* programName )
    : m_logger("NDNTrafficClient")
    , ioService_(new boost::asio::io_service)
    , face_(ioService_)
    , keyChain_()
  {
    std::srand(std::time(0));
    instanceId_ = toString(std::rand());
    programName_ = programName;
    interestInterval_ = getDefaultInterestInterval();
    interestCount_ = getDefaultInterestCount();
    configurationFile_ = "";
    totalInterestSent_ = 0;
    totalInterestReceived_ = 0;
    minimumInterestRoundTripTime_ = std::numeric_limits<double>::max();
    maximumInterestRoundTripTime_ = 0;
    totalInterestRoundTripTime_ = 0;
  }

  class InterestTrafficConfiguration
  {
  public:

    InterestTrafficConfiguration()
    {
      trafficPercentage = -1;
      name = "";
      nameAppendBytes = -1;
      nameAppendSequenceNumber = -1;
      minSuffixComponents = -1;
      maxSuffixComponents = -1;
      excludeBefore = "";
      excludeAfter = "";
      excludeBeforeBytes = -1;
      excludeAfterBytes = -1;
      childSelector = -1;
      mustBeFresh = -1;
      nonceDuplicationPercentage = -1;
      scope = -1;
      interestLifetime = -1;
      totalInterestSent = 0;
      totalInterestReceived = 0;
      minimumInterestRoundTripTime = std::numeric_limits<double>::max();
      maximumInterestRoundTripTime = 0;
      totalInterestRoundTripTime = 0;
    }

    void
    printTrafficConfiguration( Logger& logger )
    {
      std::string detail;
      detail = "";
      if (trafficPercentage > 0)
        detail += "TrafficPercentage="+toString(trafficPercentage)+", ";
      if (name != "")
        detail += "Name="+name+", ";
      if (nameAppendBytes > 0)
        detail += "NameAppendBytes="+toString(nameAppendBytes)+", ";
      if (nameAppendSequenceNumber > 0)
        detail += "NameAppendSequenceNumber="+toString(nameAppendSequenceNumber)+", ";
      if (minSuffixComponents >= 0)
        detail += "MinSuffixComponents="+toString(minSuffixComponents)+", ";
      if (maxSuffixComponents >= 0)
        detail += "MaxSuffixComponents="+toString(maxSuffixComponents)+", ";
      if (excludeBefore != "")
        detail += "ExcludeBefore="+excludeBefore+", ";
      if (excludeAfter != "")
        detail += "ExcludeAfter="+excludeAfter+", ";
      if (excludeBeforeBytes > 0)
        detail += "ExcludeBeforeBytes="+toString(excludeBeforeBytes)+", ";
      if (excludeAfterBytes > 0)
        detail += "ExcludeAfterBytes="+toString(excludeAfterBytes)+", ";
      if (childSelector >= 0)
        detail += "ChildSelector="+toString(childSelector)+", ";
      if (mustBeFresh >= 0)
        detail += "MustBeFresh="+toString(mustBeFresh)+", ";
      if (nonceDuplicationPercentage > 0)
        detail += "NonceDuplicationPercentage="+toString(nonceDuplicationPercentage)+", ";
      if (scope >= 0)
        detail += "Scope="+toString(scope)+", ";
      if (interestLifetime >= 0)
        detail += "InterestLifetime="+toString(interestLifetime)+", ";
      if (detail.length() >= 2)
        detail = detail.substr(0, detail.length()-2);
      logger.log(detail, false, false);
    }

    bool
    extractParameterValue( std::string detail, std::string& parameter, std::string& value )
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
      while ((std::isalnum(detail[i]) || allowedCharacters.find(detail[i]) != std::string::npos) && i < detail.length())
        {
          value += detail[i];
          i++;
        }
      if(parameter == "" || value == "")
        return false;
      return true;
    }

    bool
    processConfigurationDetail( std::string detail, Logger& logger, int lineNumber )
    {
      std::string parameter, value;
      if (extractParameterValue(detail, parameter, value))
        {
          if (parameter == "TrafficPercentage")
            trafficPercentage = toInteger(value);
          else if (parameter == "Name")
            name = value;
          else if (parameter == "NameAppendBytes")
            nameAppendBytes = toInteger(value);
          else if (parameter == "NameAppendSequenceNumber")
            nameAppendSequenceNumber = toInteger(value);
          else if (parameter == "MinSuffixComponents")
            minSuffixComponents = toInteger(value);
          else if (parameter == "MaxSuffixComponents")
            maxSuffixComponents = toInteger(value);
          else if (parameter == "ExcludeBefore")
            excludeBefore = value;
          else if (parameter == "ExcludeAfter")
            excludeAfter = value;
          else if (parameter == "ExcludeBeforeBytes")
            excludeBeforeBytes = toInteger(value);
          else if (parameter == "ExcludeAfterBytes")
            excludeAfterBytes = toInteger(value);
          else if (parameter == "ChildSelector")
            childSelector = toInteger(value);
          else if (parameter == "MustBeFresh")
            mustBeFresh = toInteger(value);
          else if (parameter == "NonceDuplicationPercentage")
            nonceDuplicationPercentage = toInteger(value);
          else if (parameter == "Scope")
            scope = toInteger(value);
          else if (parameter == "InterestLifetime")
            interestLifetime = toInteger(value);
          else
            logger.log("Line "+toString(lineNumber)+" \t- Invalid Parameter='"+parameter+"'", false, true);
        }
      else
        {
          logger.log("Line "+toString(lineNumber)+" \t- Improper Traffic Configuration Line- "+detail, false, true);
          return false;
        }
      return true;
    }

    bool
    checkTrafficDetailCorrectness()
    {
      return true;
    }

    int trafficPercentage;
    std::string name;
    int nameAppendBytes;
    int nameAppendSequenceNumber;
    int minSuffixComponents;
    int maxSuffixComponents;
    std::string excludeBefore;
    std::string excludeAfter;
    int excludeBeforeBytes;
    int excludeAfterBytes;
    int childSelector;
    int mustBeFresh;
    int nonceDuplicationPercentage;
    int scope;
    int interestLifetime;
    int totalInterestSent;
    int totalInterestReceived;
    double minimumInterestRoundTripTime;
    double maximumInterestRoundTripTime;
    double totalInterestRoundTripTime;

  };

  int
  getDefaultInterestLifetime()
  {
    return 4000;
  }

  static std::string
  toString( int integerValue )
  {
    std::stringstream stream;
    stream << integerValue;
    return stream.str();
  }

  static std::string
  toString( double doubleValue )
  {
    std::stringstream stream;
    stream << doubleValue;
    return stream.str();
  }

  static int
  toInteger( std::string stringValue )
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
      "Generate Interest Traffic as per provided Traffic Configuration File\n"
      "Interests are continuously generated unless a total number is specified.\n"
      "Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.\n"
      "  [-i interval] - set interest generation interval in milliseconds (minimum "
              << getDefaultInterestInterval() << " milliseconds)\n"
      "  [-c count] - set total number of interests to be generated\n"
      "  [-h] - print help and exit\n\n";
    exit(1);

  }

  int
  getDefaultInterestInterval()
  {
    return 1000;
  }

  int
  getDefaultInterestCount()
  {
    return -1;
  }

  void
  setInterestInterval( int interestInterval )
  {
    if (interestInterval < 0)
      usage();
    interestInterval_ = interestInterval;
  }

  void
  setInterestCount( int interestCount )
  {
    if (interestCount < 0)
      usage();
    interestCount_ = interestCount;
  }

  void
  setConfigurationFile( char* configurationFile )
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
    double loss, average;

    m_logger.log("\n\n== Interest Traffic Report ==\n", false, true);
    m_logger.log("Total Traffic Pattern Types = "+toString((int)trafficPattern_.size()), false, true);
    m_logger.log("Total Interests Sent        = "+toString(totalInterestSent_), false, true);
    m_logger.log("Total Responses Received    = "+toString(totalInterestReceived_), false, true);
    if (totalInterestSent_ > 0)
      loss = (totalInterestSent_-totalInterestReceived_)*100.0/totalInterestSent_;
    else
      loss = 0;
    m_logger.log("Total Interest Loss         = "+toString(loss)+"%", false, true);
    if (totalInterestReceived_ > 0)
      average = totalInterestRoundTripTime_/totalInterestReceived_;
    else
      average = 0;
    m_logger.log("Total Round Trip Time       = "+toString(totalInterestRoundTripTime_)+"ms", false, true);
    m_logger.log("Average Round Trip Time     = "+toString(average)+"ms\n", false, true);

    for (patternId=0; patternId<trafficPattern_.size(); patternId++)
      {
        m_logger.log("Traffic Pattern Type #"+toString(patternId+1), false, true);
        trafficPattern_[patternId].printTrafficConfiguration(m_logger);
        m_logger.log("Total Interests Sent        = "+toString(trafficPattern_[patternId].totalInterestSent), false, true);
        m_logger.log("Total Responses Received    = "+toString(trafficPattern_[patternId].totalInterestReceived), false, true);
        if (trafficPattern_[patternId].totalInterestSent > 0)
          {
            loss = (trafficPattern_[patternId].totalInterestSent-trafficPattern_[patternId].totalInterestReceived);
            loss *= 100.0;
            loss /= trafficPattern_[patternId].totalInterestSent;
          }
        else
          loss = 0;
        m_logger.log("Total Interest Loss         = "+toString(loss)+"%", false, true);
        if (trafficPattern_[patternId].totalInterestReceived > 0)
          average = trafficPattern_[patternId].totalInterestRoundTripTime/trafficPattern_[patternId].totalInterestReceived;
        else
          average = 0;
        m_logger.log("Total Round Trip Time       = "+toString(trafficPattern_[patternId].totalInterestRoundTripTime)+"ms", false, true);
        m_logger.log("Average Round Trip Time     = "+toString(average)+"ms\n", false, true);
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
                InterestTrafficConfiguration interestData;
                skipLine = false;
                patternId++;
                if (interestData.processConfigurationDetail(patternLine, m_logger, lineNumber))
                  {
                    while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
                      {
                        lineNumber++;
                        if (!interestData.processConfigurationDetail(patternLine, m_logger, lineNumber))
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
                    if (interestData.checkTrafficDetailCorrectness())
                      trafficPattern_.push_back(interestData);
                  }
              }
          }
        patternFile.close();
        if (!checkTrafficPatternCorrectness())
          {
            m_logger.log("ERROR - Traffic Configuration Provided Is Not Proper- " + configurationFile_, false, true);
            m_logger.shutdownLogger();
            exit(1);
          }
        m_logger.log("Traffic Configuration File Processing Completed\n", true, false);
        for (patternId=0; patternId<trafficPattern_.size(); patternId++)
          {
            m_logger.log("Traffic Pattern Type #"+toString(patternId+1), false, false);
            trafficPattern_[patternId].printTrafficConfiguration(m_logger);
            m_logger.log("", false, false);
          }
      }
    else
      {
        m_logger.log("ERROR - Unable To Open Traffic Configuration File: " + configurationFile_, false, true);
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
            m_logger.log("ERROR - Traffic Configuration File Is Not A Regular File: " + configurationFile_, false, true);
            m_logger.shutdownLogger();
            exit(1);
          }
      }
    else
      {
        m_logger.log("ERROR - Traffic Configuration File Does Not Exist: " + configurationFile_, false, true);
        m_logger.shutdownLogger();
        exit(1);
      }
  }

  int
  getOldNonce()
  {
    int randomNonceKey;
    if (nonceList_.size() == 0)
      return getNewNonce();
    std::srand(std::time(0));
    randomNonceKey = std::rand() % nonceList_.size();
    return nonceList_[randomNonceKey];
  }

  int
  getNewNonce()
  {
    int randomNonceKey, i;
    bool isOld;
    isOld = true;
    std::srand(std::time(0));
    do
      {
        randomNonceKey = std::rand();
        isOld = false;
        for (i=0; i<nonceList_.size(); i++)
          if (nonceList_[i] == randomNonceKey)
            isOld = true;
      } while(isOld);
    nonceList_.push_back(randomNonceKey);
    return randomNonceKey;
  }

  static std::string
  getRandomByteString( int randomSize )
  {
    int i;
    std::string characterSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw0123456789";
    std::string randomData;
    for (i=0; i<randomSize; i++)
      randomData += characterSet[std::rand() % characterSet.length()];
    return randomData;
  }

  void
  onData( ndn::Face &face,
          const ndn::Interest& interest,
          ndn::Data& data,
          int globalReference,
          int localReference,
          int patternId,
          boost::posix_time::ptime sentTime )
  {
    double roundTripTime;
    std::string logLine;
    logLine = "";
    logLine += "Data Received      - PatternType="+toString(patternId+1);
    logLine += ", GlobalID="+toString(globalReference);
    logLine += ", LocalID="+toString(localReference);
    logLine += ", Name="+interest.getName().toUri();
    m_logger.log(logLine, true, false);
    boost::posix_time::time_duration roundTripDuration;
    totalInterestReceived_++;
    trafficPattern_[patternId].totalInterestReceived++;
    roundTripDuration = boost::posix_time::microsec_clock::local_time() - sentTime;
    roundTripTime = roundTripDuration.total_microseconds()/1000.0;
    if (minimumInterestRoundTripTime_ > roundTripTime)
      minimumInterestRoundTripTime_ = roundTripTime;
    if (maximumInterestRoundTripTime_ < roundTripTime)
      maximumInterestRoundTripTime_ = roundTripTime;
    if (trafficPattern_[patternId].minimumInterestRoundTripTime > roundTripTime)
      trafficPattern_[patternId].minimumInterestRoundTripTime = roundTripTime;
    if (trafficPattern_[patternId].maximumInterestRoundTripTime < roundTripTime)
      trafficPattern_[patternId].maximumInterestRoundTripTime = roundTripTime;
    totalInterestRoundTripTime_ += roundTripTime;
    trafficPattern_[patternId].totalInterestRoundTripTime += roundTripTime;
    if (totalInterestSent_ == interestCount_)
      signalHandler();
  }

  void
  onTimeout( ndn::Face &face,
             const ndn::Interest& interest,
             int globalReference,
             int localReference,
             int patternId)
  {
    std::string logLine;
    logLine = "";
    logLine += "Interest Timed Out - PatternType="+toString(patternId+1);
    logLine += ", GlobalID="+toString(globalReference);
    logLine += ", LocalID="+toString(localReference);
    logLine += ", Name="+interest.getName().toUri();
    m_logger.log(logLine, true, false);
    if (totalInterestSent_ == interestCount_)
      signalHandler();
  }

  void
  generateTraffic( const boost::system::error_code& errorCode,
                   boost::asio::deadline_timer* deadlineTimer )
  {
    if ((interestCount_ < 0) || (totalInterestSent_ < interestCount_))
      {
        int trafficKey, patternId, cumulativePercentage;
        std::srand(std::time(0));
        trafficKey = std::rand() % 100;
        cumulativePercentage = 0;
        for (patternId=0; patternId<trafficPattern_.size(); patternId++)
          {
            cumulativePercentage += trafficPattern_[patternId].trafficPercentage;
            if (trafficKey <= cumulativePercentage)
              {
                Name interestName(trafficPattern_[patternId].name);
                if (trafficPattern_[patternId].nameAppendBytes > 0)
                  interestName.append(getRandomByteString(trafficPattern_[patternId].nameAppendBytes));
                if (trafficPattern_[patternId].nameAppendSequenceNumber >= 0)
                  {
                    interestName.append(toString(trafficPattern_[patternId].nameAppendSequenceNumber));
                    trafficPattern_[patternId].nameAppendSequenceNumber++;
                  }
                Interest interest(interestName);
                if (trafficPattern_[patternId].minSuffixComponents >= 0)
                  interest.setMinSuffixComponents(trafficPattern_[patternId].minSuffixComponents);
                if (trafficPattern_[patternId].maxSuffixComponents >= 0)
                  interest.setMaxSuffixComponents(trafficPattern_[patternId].maxSuffixComponents);
                Exclude exclude;
                if (trafficPattern_[patternId].excludeBefore != "" &&  trafficPattern_[patternId].excludeAfter != "")
                  {
                    exclude.excludeRange(name::Component(trafficPattern_[patternId].excludeAfter),
                                         name::Component(trafficPattern_[patternId].excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (trafficPattern_[patternId].excludeBefore != "")
                  {
                    exclude.excludeBefore(name::Component(trafficPattern_[patternId].excludeBefore));
                    interest.setExclude(exclude);
                  }
                else if (trafficPattern_[patternId].excludeAfter != "")
                  {
                    exclude.excludeAfter(name::Component(trafficPattern_[patternId].excludeAfter));
                    interest.setExclude(exclude);
                  }
                if (trafficPattern_[patternId].excludeBeforeBytes > 0 &&  trafficPattern_[patternId].excludeAfterBytes > 0)
                  {
                    exclude.excludeRange(name::Component(getRandomByteString(trafficPattern_[patternId].excludeAfterBytes)),
                                         name::Component(getRandomByteString(trafficPattern_[patternId].excludeBeforeBytes)));
                    interest.setExclude(exclude);
                  }
                else if (trafficPattern_[patternId].excludeBeforeBytes > 0)
                  {
                    exclude.excludeBefore(name::Component(getRandomByteString(trafficPattern_[patternId].excludeBeforeBytes)));
                    interest.setExclude(exclude);
                  }
                else if (trafficPattern_[patternId].excludeAfterBytes > 0)
                  {
                    exclude.excludeAfter(name::Component(getRandomByteString(trafficPattern_[patternId].excludeAfterBytes)));
                    interest.setExclude(exclude);
                  }

                if (trafficPattern_[patternId].childSelector >= 0)
                  interest.setChildSelector(trafficPattern_[patternId].childSelector);

                if (trafficPattern_[patternId].mustBeFresh == 0)
                  interest.setMustBeFresh(false);
                else if (trafficPattern_[patternId].mustBeFresh > 0)
                  interest.setMustBeFresh(true);
                if (trafficPattern_[patternId].nonceDuplicationPercentage > 0)
                  {
                    int duplicationKey;
                    std::srand(std::time(0));
                    duplicationKey = std::rand() % 100;
                    if (trafficPattern_[patternId].nonceDuplicationPercentage <= duplicationKey)
                      interest.setNonce(getOldNonce());
                    else
                      interest.setNonce(getNewNonce());
                  }
                else
                  interest.setNonce(getNewNonce());
                if (trafficPattern_[patternId].scope >= 0)
                  interest.setScope(trafficPattern_[patternId].scope);
                if (trafficPattern_[patternId].interestLifetime >= 0)
                  interest.setInterestLifetime(time::milliseconds(trafficPattern_[patternId].interestLifetime));
                else
                  interest.setInterestLifetime(time::milliseconds(getDefaultInterestLifetime()));
                try {
                  totalInterestSent_++;
                  trafficPattern_[patternId].totalInterestSent++;
                  boost::posix_time::ptime sentTime;
                  sentTime = boost::posix_time::microsec_clock::local_time();
                  face_.expressInterest(interest,
                                        func_lib::bind( &NdnTrafficClient::onData,
                                                        this, boost::ref(face_),
                                                        _1, _2, totalInterestSent_,
                                                        trafficPattern_[patternId].totalInterestSent,
                                                        patternId,
                                                        sentTime),
                                        func_lib::bind( &NdnTrafficClient::onTimeout,
                                                        this, boost::ref(face_),
                                                        _1, totalInterestSent_,
                                                        trafficPattern_[patternId].totalInterestSent,
                                                        patternId));
                  std::string logLine;
                  logLine = "";
                  logLine += "Sending Interest   - PatternType="+toString(patternId+1);
                  logLine += ", GlobalID="+toString(totalInterestSent_);
                  logLine += ", LocalID="+toString(trafficPattern_[patternId].totalInterestSent);
                  logLine += ", Name="+interest.getName().toUri();
                  m_logger.log(logLine, true, false);
                  deadlineTimer->expires_at(deadlineTimer->expires_at() +
                                            boost::posix_time::millisec(interestInterval_));
                  deadlineTimer->async_wait(boost::bind(&NdnTrafficClient::generateTraffic,
                                                        this,
                                                        boost::asio::placeholders::error,
                                                        deadlineTimer));
                }
                catch (std::exception &e) {
                  m_logger.log("ERROR: "+(std::string)e.what(), true, true);
                }
                break;
              }
          }
        if (patternId==trafficPattern_.size())
          {
            deadlineTimer->expires_at(deadlineTimer->expires_at() +
                                      boost::posix_time::millisec(interestInterval_));
            deadlineTimer->async_wait(boost::bind(&NdnTrafficClient::generateTraffic,
                                                  this,
                                                  boost::asio::placeholders::error,
                                                  deadlineTimer));
          }
      }
  }

  void
  initialize()
  {
    boost::asio::signal_set signalSet(*ioService_, SIGINT, SIGTERM);
    signalSet.async_wait(boost::bind(&NdnTrafficClient::signalHandler, this));
    m_logger.initializeLog(instanceId_);
    initializeTrafficConfiguration();
    boost::asio::deadline_timer deadlineTimer(*ioService_,
                                              boost::posix_time::millisec(interestInterval_));
    deadlineTimer.async_wait(boost::bind(&NdnTrafficClient::generateTraffic,
                                         this,
                                         boost::asio::placeholders::error,
                                         &deadlineTimer));
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
  int interestInterval_;
  int interestCount_;
  Logger m_logger;
  std::string configurationFile_;
  ptr_lib::shared_ptr<boost::asio::io_service> ioService_;
  Face face_;
  std::vector<InterestTrafficConfiguration> trafficPattern_;
  std::vector<int> nonceList_;
  int totalInterestSent_;
  int totalInterestReceived_;
  double minimumInterestRoundTripTime_;
  double maximumInterestRoundTripTime_;
  double totalInterestRoundTripTime_;

};

} // namespace ndn

int main( int argc, char* argv[] )
{
  int option;
  ndn::NdnTrafficClient ndnTrafficClient (argv[0]);
  while ((option = getopt(argc, argv, "hi:c:")) != -1) {
    switch (option) {
    case 'h'  :
      ndnTrafficClient.usage();
      break;
    case 'i'  :
      ndnTrafficClient.setInterestInterval(atoi(optarg));
      break;
    case 'c'  :
      ndnTrafficClient.setInterestCount(atoi(optarg));
      break;
    default   :
      ndnTrafficClient.usage();
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == NULL)
    ndnTrafficClient.usage();

  ndnTrafficClient.setConfigurationFile(argv[0]);
  ndnTrafficClient.initialize();

  return 0;
}
