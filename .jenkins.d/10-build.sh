#!/usr/bin/env bash
set -exo pipefail

# Build in debug mode
./waf --color=yes configure --debug
./waf --color=yes build

# Cleanup
./waf --color=yes distclean

# Build in release mode
./waf --color=yes configure
./waf --color=yes build

# Install
sudo ./waf --color=yes install
