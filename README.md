Traffic Generator for NDN (ndn-traffic-generator)
=================================================

[![Build Status](https://travis-ci.org/named-data/ndn-traffic-generator.svg?branch=master)](https://travis-ci.org/named-data/ndn-traffic-generator)

This tool is designed to generate Interest and Data traffic in an NDN network.
The client and server tool accept traffic configuration files which can be
used to specify the pattern of NDN traffic that is required to be generated.
Sample configuration files are provided which include instructions on how
to configure various parameters.

## Prerequisites ##

Compiling and running ndn-traffic-generator requires the following dependencies:

1. ndn-cxx library <https://github.com/named-data/ndn-cxx>

    For detailed installation instructions, please see
    [`INSTALL.rst`](https://github.com/named-data/ndn-cxx/blob/master/docs/INSTALL.rst)

2. NDN forwarding daemon <https://github.com/named-data/NFD>

-----------------------------------------------------

## 1. Compilation & Installation Instructions: ##

    ./waf configure
    ./waf
    sudo ./waf install


## 2. Tool Run Instructions & Command Line Options: ##

    Usage: ndn-traffic-server [options] <traffic_configuration_file>
    Respond to Interest as per provided traffic configuration file
    Multiple prefixes can be configured for handling.
    Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.
      [-d interval] - set delay before responding to interest in milliseconds
      [-c count]    - specify maximum number of interests to be satisfied
      [-q]          - quiet logging - no interest reception/data generation messages
      [-h]          - print help and exit

    Usage: ndn-traffic [options] <traffic_configuration_file>
    Generate Interest traffic as per provided traffic configuration file
    Interests are continuously generated unless a total number is specified.
    Set environment variable NDN_TRAFFIC_LOGFOLDER for redirecting output to a log.
      [-i interval] - set interest generation interval in milliseconds (default 1000 milliseconds)
      [-c count]    - set total number of interests to be generated
      [-q]          - quiet logging - no interest reception/data generation messages
      [-h]          - print help and exit


## 3. Sample Run Instructions ##

__ON MACHINE #1__

(NFD must be running)

Start traffic server:

        ndn-traffic-server ndn-traffic-server.conf

__ON MACHINE #2__

(NFD must be running)

Start the traffic client:

        ndn-traffic ndn-traffic-client.conf


* These tools need not be used together and can be used individually as well.
* Please refer sample configuration files provided for details on how to create your own.
* Use command line options shown above to adjust traffic configuration.
