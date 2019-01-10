#!/usr/bin/env bash
set -e

JDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$JDIR"/util.sh

set -x

sudo_preserve_env PATH -- ./waf --color=yes distclean

./waf --color=yes configure
./waf --color=yes build -j${WAF_JOBS:-1}

sudo_preserve_env PATH -- ./waf --color=yes install
