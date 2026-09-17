#!/bin/bash

set -e

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  bpfcc-tools \
  python-is-python3
