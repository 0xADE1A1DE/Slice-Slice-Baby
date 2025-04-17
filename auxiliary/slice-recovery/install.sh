#!/bin/bash

sudo apt update
sudo apt install build-essential

pushd ./perfcounters
./install.sh
popd

./build.sh
