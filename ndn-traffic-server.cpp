/**
 *
 * Copyright (C) 2013 University of Arizona.
 * @author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 *
 */

#include <ndn-cpp-dev/security/key-chain.hpp>

using namespace ndn;

class NdnTrafficServer
{
public:

  NdnTrafficServer( char* programName )
  {
    programName_ = programName;
    contentDelayTime_ = getDefaultContentDelayTime();
    prefix_ = "";
    configurationFile_ = "";
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

  bool
  isPrefixSet()
  {
    if (prefix_.length() > 0)
      return true;
    else
      return false;
  }

  void
  setContentDelayTime( int contentDelayTime )
  {
    if (contentDelayTime < 0)
      usage();
    contentDelayTime_ = contentDelayTime;
  }

  void
  setPrefix( char* prefix )
  {
    prefix_ = prefix;
  }

  void
  setConfigurationFile( char* configurationFile )
  {
    configurationFile_ = configurationFile;
  }

private:

  KeyChain keyChain_;
  std::string programName_;
  int contentDelayTime_;
  std::string prefix_;
  std::string configurationFile_;

};

int main( int argc, char* argv[] )
{
  int option;
  NdnTrafficServer ndnTrafficServer (argv[0]);
  while ((option = getopt(argc, argv, "hd:p:")) != -1) {
    switch (option) {
      case 'h'  :
        ndnTrafficServer.usage();
        break;
      case 'd'  :
        ndnTrafficServer.setContentDelayTime(atoi(optarg));
        break;
      case 'p'  :
        ndnTrafficServer.setPrefix(optarg);
        break;
      default   :
        ndnTrafficServer.usage();
        break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == NULL && !ndnTrafficServer.isPrefixSet() )
    ndnTrafficServer.usage();

  ndnTrafficServer.setConfigurationFile(argv[0]);

  return 0;
}
