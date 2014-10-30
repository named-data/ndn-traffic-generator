#!/usr/bin/env bash
set -x
set -e

sudo ./waf distclean -j1 --color=yes
./waf configure -j1 --color=yes
./waf -j1 --color=yes
sudo ./waf install -j1 --color=yes
