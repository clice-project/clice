# Prepare Development Environment

This document indicates how to build clice in  Linux / WSL environment.

## Prepare Requirements

The following tools need to be installed: 

+ clang & clang++   
Which is required to support the `C++23` standard to build clice itself and submodule LLVM. 

+ libstdc++-14-dev   
Supply C++ standard library headers like `<format>`, by `sudo apt install libstdc++-14-dev`.       
You can also install gcc-14 to prepare this by `sudo apt install gcc-14` (e.g Ubuntu 24.04.1)

+ cmake    
Minimum required version is `3.22` .

+ lld    
Clice use lld as the linker to build llvm. 

+ ninja 



## Download Source Code 

Use the following commands to get source code of clice quickly:

``` sh
git clone https://github.com/clice-project/clice.git 
cd clice 
git submodule init 
git submodule update --recursive -depth 1 
```

## Build Dependency 

Clice relies on a specific version of Clang (and it's inner headers), which be updated manually by developer. So LLVM is one of the submodules of clice.     

Use the following commands to compile LLVM and extract clang inner headers to build artifacts directory. 

``` sh 
cd clice 
python3 scripts/build-llvm-dev.py
```

If you are not the first time to build LLVM that clice relied on (e.g recently synced the fork from upstream), make sure that the cache of compilation was cleared successfully before another build. Use `rm -r deps/llvm/build-install` to remove it.

## Build Clice 

Build clice with following commands:

``` sh
cd clice 

# build clice without test 
./scripts/build-dev.sh

# or build with test 
# ./scripts/build-dev-test.sh

cmake --build build -j
```

## Run Test  

Run test with commands: 

``` sh
./build/bin/clice-tests --test-dir tests     
```

And get outputs like 

```
[==========] Running 50 tests from 8 test suites.
[----------] Global test environment set-up.
[----------] 4 tests from URITest
[ RUN      ] URITest.ConstructorAndAccessors
[       OK ] URITest.ConstructorAndAccessors (0 ms)
[ RUN      ] URITest.CopyConstructor
[       OK ] URITest.CopyConstructor (0 ms)
[ RUN      ] URITest.EqualityOperator
[       OK ] URITest.EqualityOperator (0 ms)
[ RUN      ] URITest.ParseFunction
[       OK ] URITest.ParseFunction (0 ms)
[----------] 4 tests from URITest (0 ms total)
...
[----------] Global test environment tear-down
[==========] 47 tests from 8 test suites ran. (2021 ms total)
[  PASSED  ] 47 tests.
```

