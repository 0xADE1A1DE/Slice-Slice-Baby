#!/bin/bash

sudo apt install build-essential autoconf pkg-config libtool stress-ng

cd "$(git rev-parse --show-toplevel)" || {
  echo "Failed to find git root. Are you inside a git repo?"
  exit 1
}

echo "Initialising Git submodules..."
git submodule init
git submodule update --recursive --remote

pushd ./lib/AssemblyLine
./autogen.sh
./configure
make -j
sudo make install
popd

sudo ldconfig

if [[ $? -gt 0 ]]; then
	exit 1
fi

cd scripts/
echo "Done!"
