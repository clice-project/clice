#! /usr/bin/python3

import os
import shutil
import subprocess

os.chdir("deps/llvm")

args = [
    "-B=build",
    "-S=./llvm",
    "-G=Ninja",
    "-DLLVM_USE_LINKER=lld",
    "-DCMAKE_C_COMPILER=clang",
    "-DCMAKE_CXX_COMPILER=clang++",
    # "-DBUILD_SHARED_LIBS=ON",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DLLVM_TARGETS_TO_BUILD=X86",
    "-DLLVM_ENABLE_PROJECTS=clang",
    "-DCMAKE_INSTALL_PREFIX=./build-install",
]

subprocess.run(["cmake"] + args)
subprocess.run(["cmake", "--build", "build", "--target", "clang"])
subprocess.run(["cmake", "--build", "build", "--target", "install"])

src = "./clang/lib/Sema/"
dst = "./build-install/include/clang/Sema/"

for file in ["CoroutineStmtBuilder.h", "TypeLocBuilder.h", "TreeTransform.h"]:
    shutil.copyfile(src + file, dst + file)
    print(f"Copying {src + file} to {dst + file}")
