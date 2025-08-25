#!/usr/bin/env bash

# Online Documentation: https://clice.io/dev/build.html
# This is a sample script to install the prebuilt LLVM binaries for Linux.
# The sample script is used in the GitHub Actions and Dockerfiles.

mkdir -p ./.llvm
curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/x86_64-linux-gnu-release.tar.xz" | tar -xJ -C ./.llvm
