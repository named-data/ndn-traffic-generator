/**
 *
 * Copyright (C) 2014 University of Arizona.
 * @author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 *
 */

#include <sstream>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <ndn-cpp-dev/face.hpp>
#include <ndn-cpp-dev/security/key-chain.hpp>

using namespace ndn;

class NdnTrafficServer
{
public:

  NdnTrafficServer( char* programName )
  {
    std::stringstream randomId;
    std::srand(std::time(0));
    randomId << std::rand();
    instanceId_ = randomId.str();
    programName_ = programName;
    contentDelayTime_ = getDefaultContentDelayTime();
    logLocation_ = "";
    configurationFile_ = "";
    ioService_ = ptr_lib::make_shared<boost::asio::io_service>();
    face_ = Face(ioService_);
  }

  NdnTrafficServer()
  : keyChain_()
  {
  }

  void
  usage()
  {
    std::cout << "\nUsage: " << programName_ << " Printing Usage"
      "\n\n";
    exit(1);
  }

  int
  getDefaultContentDelayTime()
  {
    return 0;
  }

  void
  setContentDelayTime( int contentDelayTime )
  {
    if (contentDelayTime < 0)
      usage();
    contentDelayTime_ = contentDelayTime;
  }

  void
  setConfigurationFile( char* configurationFile )
  {
    configurationFile_ = configurationFile;
  }

  void
  signalHandler()
  {
    face_.shutdown();
    ioService_.reset();
    exit(1);
  }

  void
  initializeLog()
  {
    char* variableValue = std::getenv("NDN_TRAFFIC_LOGFOLDER");
    if (variableValue != NULL)
      logLocation_ = variableValue;

    if (boost::filesystem::exists(boost::filesystem::path(logLocation_)))
    {
      if (boost::filesystem::is_directory(boost::filesystem::path(logLocation_)))
      {
        logLocation_ += "/NDNTrafficServer_"+instanceId_+".log";
        std::cout << "Log File Initialized: " << logLocation_ << std::endl;
      }
      else
      {
        std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Should Be A Folder.\n"
                     "Using Default Output For Logging." << std::endl;
        logLocation_ = "";
      }
    }
    else
    {
      std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Not Set.\n"
                   "Using Default Output For Logging." << std::endl;
      logLocation_ = "";
    }
  }

  void
  initializeTrafficConfiguration()
  {
    std::cout << "Traffic Configuration File: " << configurationFile_ << std::endl;
  }

  void
  initialize()
  {
    boost::asio::signal_set signalSet(*ioService_, SIGINT, SIGTERM);
    signalSet.async_wait(boost::bind(&NdnTrafficServer::signalHandler, this));
    initializeLog();
    initializeTrafficConfiguration();
  }

private:

  KeyChain keyChain_;
  std::string programName_;
  int contentDelayTime_;
  std::string logLocation_;
  std::string configurationFile_;
  ptr_lib::shared_ptr<boost::asio::io_service> ioService_;
  Face face_;
  std::string instanceId_;

};

int main( int argc, char* argv[] )
{
  int option;
  NdnTrafficServer ndnTrafficServer (argv[0]);
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
