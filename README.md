Traffic Generator For NDN (ndn-traffic)
=======================================

This tool is designed to generate interest and data traffic in an NDN network.
The client and server tool accept traffic configuration files which can be
used to specify the pattern of NDN traffic that is required to be generated.
Sample configuration files are provided which include instructions on how 
to configure various parameters. 

This is an application tool developed over ndn-cpp APIs for TLV packet
based communication over NDN configured network. The NDN network has to
be first created with 'ndnx' and 'ndnd-tlv'. This is followed by environment
security setup with 'ndn-cpp-security-tools'. The application also requires
installation of ndn-cpp and CPP boost libraries. The installations need to
follow a strict order as the ndnd

To run the following must be ensured(FOLLOW ORDER STRICTLY)

1. Install ndnx (install all necessary dependencies)
2. Install CPP Boost Library > 1.47

        sudo apt-get install libboost1.48-all-dev

3. Install ndn-cpp-dev (install all necessary dependencies except boost)
4. Install ndnd-tlv install all necessary dependencies except boost)
5. Install and Configure Security Environment with ndn-cpp-security-tools

-----------------------------------------------------

## 1. Compile And Installation Instructions: ##

    git clone git://github.com/jeraldabraham/ndn-traffic ndn-traffic
    cd ndn-traffic
    ./waf configure 
    ./waf
    sudo ./waf install

## 2. Tool Run Instructions & Command Line Options: ##

    Usage: ndntrafficserver [options] <Traffic_Configuration_File>
    Respond to Interest as per provided Traffic Configuration File
    Multiple Prefixes can be configured for handling.
    Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.
      [-d interval] - set delay before responding to interest in milliseconds (minimum 0 milliseconds)
      [-h] - print help and exit

    Usage: ndntraffic [options] <Traffic_Configuration_File>
    Generate Interest Traffic as per provided Traffic Configuration File
    Interests are continuously generated unless a total number is specified.
    Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.
      [-i interval] - set interest generation interval in milliseconds (minimum 1000 milliseconds)
      [-c count] - set total number of interests to be generated
      [-h] - print help and exit


## 3. Sample Run Instructions ##

__ON MACHINE #1__

Start the ndnd-tlv daemon

        ndnd-tlv-start

Start traffic server

        ndntrafficserver NDNTrafficServer.conf

__ON MACHINE #2__

Start the ndnd-tlv daemon

        ndnd-tlv-start

Start the traffic client
        
        ndntraffic NDNTrafficClient.conf


* These tools need not be used together and can be used individually as well.
* Please refer sample configuration files provided for details on how to create your own.
* Use command line options shown above to adjust traffic configuration.

