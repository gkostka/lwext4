#!/usr/bin/env bash

sudo apt-get install -y make gcc cmake p7zip
make generic
cd build_generic
make
