#!/usr/bin/env bash
set -e
set -x

sudo env "PATH=$PATH" ./waf --color=yes distclean

./waf --color=yes configure
./waf --color=yes build -j${WAF_JOBS:-1}

sudo env "PATH=$PATH" ./waf --color=yes install
