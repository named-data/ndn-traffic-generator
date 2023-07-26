# NDN Traffic Generator

[![CI](https://github.com/named-data/ndn-traffic-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/named-data/ndn-traffic-generator/actions/workflows/ci.yml)
![Language](https://img.shields.io/badge/C%2B%2B-17-blue)

This tool is designed to generate Interest and Data traffic in an NDN network.
The client and server tool accept traffic configuration files which can be
used to specify the pattern of NDN traffic that is required to be generated.
Sample configuration files are provided which include instructions on how
to configure various parameters.

## Prerequisites

Compiling and running ndn-traffic-generator requires the following dependencies:

1. [ndn-cxx and its dependencies](https://docs.named-data.net/ndn-cxx/current/INSTALL.html)
2. [NDN Forwarding Daemon (NFD)](https://docs.named-data.net/NFD/current/INSTALL.html)

## Compilation & Installation

```shell
./waf configure
./waf
sudo ./waf install
```

## Command Line Options

### `ndn-traffic-server`

    Usage: ndn-traffic-server [options] <Traffic_Configuration_File>

    Respond to Interests as per provided Traffic_Configuration_File.
    Multiple prefixes can be configured for handling.
    Set the environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.

    Options:
      -h [ --help ]                 print this help message and exit
      -c [ --count ] arg            maximum number of Interests to respond to
      -d [ --delay ] arg (=0)       wait this amount of milliseconds before responding to each Interest
      -t [ --timestamp-format ] arg format string for timestamp output (see below)
      -q [ --quiet ]                turn off logging of Interest reception and Data generation

### `ndn-traffic-client`

    Usage: ndn-traffic-client [options] <Traffic_Configuration_File>

    Generate Interest traffic as per provided Traffic_Configuration_File.
    Interests are continuously generated unless a total number is specified.
    Set the environment variable NDN_TRAFFIC_LOGFOLDER to redirect output to a log file.

    Options:
      -h [ --help ]                 print this help message and exit
      -c [ --count ] arg            total number of Interests to be generated
      -i [ --interval ] arg (=1000) Interest generation interval in milliseconds
      -t [ --timestamp-format ] arg format string for timestamp output (see below)
      -q [ --quiet ]                turn off logging of Interest generation and Data reception
      -v [ --verbose ]              log additional per-packet information

* These tools need not be used together and can be used individually as well.
* Please refer to the sample configuration files provided for details on how to create your own.
* Use the command line options shown above to adjust traffic configuration.
* By default, timestamps are logged in Unix epoch format with microsecond granularity.
  For custom output, the `--timestamp-format` option expects a format string using the syntax given in the
  [Boost.Date_Time documentation](https://www.boost.org/doc/libs/1_71_0/doc/html/date_time/date_time_io.html#date_time.format_flags).

## Example

#### ON MACHINE #1

(NFD must be running)

Start the traffic server:

```shell
ndn-traffic-server ndn-traffic-server.conf
```

#### ON MACHINE #2

(NFD must be running)

Start the traffic client:

```shell
ndn-traffic-client ndn-traffic-client.conf
```

## License

ndn-traffic-generator is free software distributed under the GNU General Public License version 3.
See [`COPYING.md`](COPYING.md) for details.
