#!/usr/bin/env bash

# Online Documentation: https://clice.io/dev/build.html
# This is a sample script to install the prebuilt LLVM binaries for Linux and macOS.
# The sample script is used in the GitHub Actions and Dockerfiles.

mkdir -p ./.llvm

if [ "$(uname -m)" = "x86_64" ] && [ "$(uname -s)" = "Linux" ]; then
  curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/x86_64-linux-gnu-release.tar.xz" | tar -xJ -C ./.llvm
elif [ "$(uname -m)" = "arm64" ] && [ "$(uname -s)" = "Darwin" ]; then
  curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-debug.tar.xz" | tar -xJ -C ./.llvm
else
  echo "Unsupported platform: $(uname -m)-$(uname -s)"
  exit 1
fi
