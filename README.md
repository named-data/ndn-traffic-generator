Traffic Generator For NDN (ndn-traffic-generator)
=================================================

This tool is designed to generate interest and data traffic in an NDN network.
The client and server tool accept traffic configuration files which can be
used to specify the pattern of NDN traffic that is required to be generated.
Sample configuration files are provided which include instructions on how
to configure various parameters.

## Prerequisites ##

Compiling and running ndn-traffic-generator requires the following dependencies:

1. C++ Boost Libraries version >= 1.48

    On Ubuntu 12.04:

        sudo apt-get install libboost1.48-all-dev

    On Ubuntu 13.10 and later

        sudo apt-get install libboost-all-dev

    On OSX with macports

        sudo port install boost

    On OSX with brew

        brew install boost

    On other platforms Boost Libraries can be installed from the packaged version for the
    distribution, if the version matches requirements, or compiled from source
    (http://www.boost.org)

2. ndn-cpp-dev library (https://github.com/named-data/ndn-cpp-dev)

    For detailed installation instructions refer to
    https://github.com/named-data/ndn-cpp-dev/blob/master/INSTALL.md and/or
    https://github.com/named-data/ndn-cpp-dev

3. NDN forwarding daemon (https://github.com/named-data/NFD)

-----------------------------------------------------

## 1. Compile And Installation Instructions: ##

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

(NDN forwarding daemon should be running)

Start traffic server

        ndn-traffic-server ndn-traffic-server.conf

__ON MACHINE #2__

(NDN forwarding daemon should be running)

Start the traffic client

        ndn-traffic ndn-traffic-client.conf


* These tools need not be used together and can be used individually as well.
* Please refer sample configuration files provided for details on how to create your own.
* Use command line options shown above to adjust traffic configuration.
