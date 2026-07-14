#!/bin/bash

clear
mkdir -p build
cd build
rm -rf *
cmake ../
make -j4 || exit 1
echo "Build done at $(date)"
