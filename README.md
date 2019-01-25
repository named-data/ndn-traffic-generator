# ndn-traffic-generator: Traffic Generator for NDN

[![Build Status](https://travis-ci.org/named-data/ndn-traffic-generator.svg?branch=master)](https://travis-ci.org/named-data/ndn-traffic-generator)

This tool is designed to generate Interest and Data traffic in an NDN network.
The client and server tool accept traffic configuration files which can be
used to specify the pattern of NDN traffic that is required to be generated.
Sample configuration files are provided which include instructions on how
to configure various parameters.


## Prerequisites

Compiling and running ndn-traffic-generator requires the following dependencies:

1. ndn-cxx library <https://github.com/named-data/ndn-cxx>

    For detailed installation instructions, please see
    [`INSTALL.rst`](https://github.com/named-data/ndn-cxx/blob/master/docs/INSTALL.rst)

2. NDN forwarding daemon <https://github.com/named-data/NFD>


## Compilation & Installation

    ./waf configure
    ./waf
    sudo ./waf install


## Command Line Options

#### ndn-traffic-server

    Usage: ndn-traffic-server [options] <Traffic_Configuration_File>
    Respond to Interests as per provided Traffic_Configuration_File.
    Multiple prefixes can be configured for handling.
    Set the environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.
    Options:
      -h [ --help ]           print this help message and exit
      -c [ --count ] arg      maximum number of Interests to respond to
      -d [ --delay ] arg (=0) wait this amount of milliseconds before responding to each Interest
      -q [ --quiet ]          turn off logging of Interest reception/Data generation

#### ndn-traffic-client

    Usage: ndn-traffic-client [options] <Traffic_Configuration_File>
    Generate Interest traffic as per provided Traffic_Configuration_File.
    Interests are continuously generated unless a total number is specified.
    Set the environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.
    Options:
      -h [ --help ]                 print this help message and exit
      -c [ --count ] arg            total number of Interests to be generated
      -i [ --interval ] arg (=1000) Interest generation interval in milliseconds
      -q [ --quiet ]                turn off logging of Interest generation/Data reception

* These tools need not be used together and can be used individually as well.
* Please refer to the sample configuration files provided for details on how to create your own.
* Use the command line options shown above to adjust traffic configuration.


### Sample Run Instructions

##### ON MACHINE #1

(NFD must be running)

Start the traffic server:

        ndn-traffic-server ndn-traffic-server.conf

##### ON MACHINE #2

(NFD must be running)

Start the traffic client:

        ndn-traffic-client ndn-traffic-client.conf
