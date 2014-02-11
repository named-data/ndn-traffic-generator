/**
 *
 * Copyright (C) 2013 University of Arizona.
 * @author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 *
 */

#include <ndn-cpp-dev/security/key-chain.hpp>

using namespace ndn;

class NdnTrafficClient
{
public:

  NdnTrafficClient( char* programName )
  {
    programName_ = programName;
    interestInterval_ = getDefaultInterestInterval();
    interestCount_ = getDefaultInterestCount();
    prefix_ = "";
    configurationFile_ = "";
  }

  NdnTrafficClient()
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
  getDefaultInterestInterval()
  {
    return -1;
  }

  int
  getDefaultInterestCount()
  {
    return -1;
  }

  bool
  isPrefixSet()
  {
    if ( prefix_.length() > 0 )
      return true;
    else
      return false;
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
  int interestInterval_;
  int interestCount_;
  std::string prefix_;
  std::string configurationFile_;

};

int main( int argc, char* argv[] )
{
  int option;
  NdnTrafficClient ndnTrafficClient (argv[0]);
  while ((option = getopt(argc, argv, "hi:c:p:")) != -1) {
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
      case 'p'  :
        ndnTrafficClient.setPrefix(optarg);
        break;
      default   :
        ndnTrafficClient.usage();
        break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argv[0] == NULL && !ndnTrafficClient.isPrefixSet() )
    ndnTrafficClient.usage();

  ndnTrafficClient.setConfigurationFile(argv[0]);

  return 0;
}
