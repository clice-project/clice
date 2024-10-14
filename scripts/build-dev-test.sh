#! /usr/bin/bash

cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCLICE_ENABLE_TEST=ON -DCMAKE_CXX_FLAGS="-fno-rtti -g -O0"