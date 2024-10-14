#! /usr/bin/bash

cd deps/llvm
cmake -B build-release -S ./llvm -G Ninja \
-DCMAKE_BUILD_TYPE=Release \
-DLLVM_ENABLE_PROJECTS="clang" \
-DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -w" \
-DCMAKE_C_COMPILER=clang \
-DCMAKE_CXX_COMPILER=clang++ \
-DLLVM_USE_LINKER=lld \
-DLLVM_ENABLE_PROJECTS=clang \
-DCMAKE_INSTALL_PREFIX=./build-install

cmake --build build-release
cmake --build build-release --target install