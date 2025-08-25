#!/usr/bin/env bash

# Online Documentation: https://clice.io/dev/build.html
# This is a sample script to build the project.
# You could copy this script using the following command and modify it to your needs.
#
# ## Example: Release build
#
# ```bash
# cp scripts/build.sh scripts/build-release.sh && \
#  sed -i 's/DCMAKE_BUILD_TYPE=Debug/DCMAKE_BUILD_TYPE=Release/g' scripts/build-release.sh
# ```

# Specify the `LLVM_INSTALL_PATH` to use a custom LLVM build.
LLVM_INSTALL_PATH="$PWD/.llvm"
# Specify the build directory.
BUILD_DIR="build"
# Specify the compiler.
COMPILER_FLAGS="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"

cmake -B "$BUILD_DIR" -G Ninja -DLLVM_INSTALL_PATH="$LLVM_INSTALL_PATH" $COMPILER_FLAGS -DCMAKE_BUILD_TYPE=Debug -DCLICE_ENABLE_TEST=ON
cmake --build "$BUILD_DIR" -j
