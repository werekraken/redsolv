#!/bin/bash

set -e

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  build-essential

cd /vagrant/helpers
make clean
make
make install
