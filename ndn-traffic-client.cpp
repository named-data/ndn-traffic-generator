/**
 *
 * Copyright (C) 2014 University of Arizona.
 * @author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 *
 */

#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <ndn-cpp-dev/face.hpp>
#include <ndn-cpp-dev/security/key-chain.hpp>

using namespace ndn;

class Logger
{
public:

  Logger()
  {
    logLocation_ = "";
  }

  void
  shutdownLogger()
  {
    if (logFile_.is_open())
      logFile_.close();
  }

  void
  log( std::string logLine )
  {
    if( logLocation_.length() > 0 )
    {
      logFile_ << logLine << std::endl;
      logFile_.flush();
    }
    else
      std::cout << logLine << std::endl;
  }

  void
  initializeLog( std::string instanceId )
  {
    char* variableValue = std::getenv("NDN_TRAFFIC_LOGFOLDER");
    std::string logFilename;
    logLocation_ = "";
    if (variableValue != NULL)
      logLocation_ = variableValue;
    if (boost::filesystem::exists(boost::filesystem::path(logLocation_)))
    {
      if (boost::filesystem::is_directory(boost::filesystem::path(logLocation_)))
      {
        logFilename = logLocation_+"/NDNTrafficClient_"+instanceId+".log";
        logFile_.open(logFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
        if (logFile_.is_open())
          std::cout << "Log File Initialized: " << logFilename << std::endl;
        else
        {
          std::cout << "ERROR - Unable To Initialize A Log File At: " << logLocation_ << std::endl
                    << "Using Default Output For Logging." << std::endl;
          logLocation_ = "";
        }
      }
      else
      {
        std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Should Be A Folder." << std::endl
                  << "Using Default Output For Logging." << std::endl;
        logLocation_ = "";
      }
    }
    else
    {
      std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Not Set." << std::endl
                << "Using Default Output For Logging." << std::endl;
      logLocation_ = "";
    }
  }

private:

  std::string logLocation_;
  std::ofstream logFile_;

};

class NdnTrafficClient
{
public:

  NdnTrafficClient( char* programName )
  {
    std::srand(std::time(0));
    instanceId_ = toString(std::rand());
    programName_ = programName;
    interestInterval_ = getDefaultInterestInterval();
    interestCount_ = getDefaultInterestCount();
    configurationFile_ = "";
    ioService_ = ptr_lib::make_shared<boost::asio::io_service>();
    face_ = Face(ioService_);
  }

  NdnTrafficClient()
  : keyChain_()
  {
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
      if (nameAppendBytes>0)
        detail += "NameAppendBytes="+toString(nameAppendBytes)+", ";
      if (nameAppendSequenceNumber>0)
        detail += "NameAppendSequenceNumber="+toString(nameAppendSequenceNumber)+", ";
      if (minSuffixComponents>0)
        detail += "MinSuffixComponents="+toString(minSuffixComponents)+", ";
      if (maxSuffixComponents>0)
        detail += "MaxSuffixComponents="+toString(maxSuffixComponents)+", ";
      if (excludeBefore != "")
        detail += "ExcludeBefore="+excludeBefore+", ";
      if (excludeAfter != "")
        detail += "ExcludeAfter="+excludeAfter+", ";
      if (excludeBeforeBytes > 0)
        detail += "ExcludeBeforeBytes="+toString(excludeBeforeBytes)+", ";
      if (excludeAfterBytes > 0)
        detail += "ExcludeAfterBytes="+toString(excludeAfterBytes)+", ";
      if (childSelector > 0)
        detail += "ChildSelector="+toString(childSelector)+", ";
      if (mustBeFresh > 0)
        detail += "MustBeFresh="+toString(mustBeFresh)+", ";
      if (nonceDuplicationPercentage > 0)
        detail += "NonceDuplicationPercentage="+toString(nonceDuplicationPercentage)+", ";
      if (scope > 0)
        detail += "Scope="+toString(scope)+", ";
      if (interestLifetime > 0)
        detail += "InterestLifetime="+toString(interestLifetime)+", ";
      if (detail.length() >= 0)
        detail = detail.substr(0, detail.length()-2);
      logger.log(detail);
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
          logger.log("Line "+toString(lineNumber)+" \t- Invalid Parameter='"+parameter+"'");
      }
      else
      {
        logger.log("Line "+toString(lineNumber)+" \t: Improper Traffic Configuration Line- "+detail);
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

  };

  static std::string
  toString( int integerValue )
  {
    std::stringstream stream;
    stream << integerValue;
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
    std::cout << "\nUsage: " << programName_ << " Printing Usage"
              << std::endl << std::endl;
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
    logger_.shutdownLogger();
    face_.shutdown();
    ioService_.reset();
    exit(1);
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
    logger_.log("Analyzing Traffic Configuration File: " + configurationFile_);
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
          if (interestData.processConfigurationDetail(patternLine, logger_, lineNumber))
          {
            while (getline(patternFile, patternLine) && std::isalpha(patternLine[0]))
            {
              lineNumber++;
              if (!interestData.processConfigurationDetail(patternLine, logger_, lineNumber))
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
        logger_.log("ERROR - Traffic Configuration Provided Is Not Proper- " + configurationFile_);
        logger_.shutdownLogger();
        std::cout << "ERROR - Check Log File For Details." << std::endl;
        exit(1);
      }
      for (patternId = 0; patternId < trafficPattern_.size(); patternId++)
        trafficPattern_[patternId].printTrafficConfiguration(logger_);
    }
    else
    {
      logger_.log("ERROR - Unable To Open Traffic Configuration File: " + configurationFile_);
      logger_.shutdownLogger();
      std::cout << "ERROR - Check Log File For Details." << std::endl;
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
        logger_.log("ERROR - Traffic Configuration File Is Not A Regular File: " + configurationFile_);
        logger_.shutdownLogger();
        std::cout << "ERROR - Check Log File For Details." << std::endl;
        exit(1);
      }
    }
    else
    {
      logger_.log("ERROR - Traffic Configuration File Does Not Exist: " + configurationFile_);
      logger_.shutdownLogger();
      std::cout << "ERROR - Check Log File For Details." << std::endl;
      exit(1);
    }
  }

  void
  initialize()
  {
    boost::asio::signal_set signalSet(*ioService_, SIGINT, SIGTERM);
    signalSet.async_wait(boost::bind(&NdnTrafficClient::signalHandler, this));
    logger_.initializeLog(instanceId_);
    initializeTrafficConfiguration();
  }

private:

  KeyChain keyChain_;
  std::string programName_;
  std::string instanceId_;
  int interestInterval_;
  int interestCount_;
  Logger logger_;
  std::string configurationFile_;
  ptr_lib::shared_ptr<boost::asio::io_service> ioService_;
  std::vector<InterestTrafficConfiguration> trafficPattern_;
  Face face_;

};

int main( int argc, char* argv[] )
{
  int option;
  NdnTrafficClient ndnTrafficClient (argv[0]);
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
