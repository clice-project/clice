cmake -B build-release -G Ninja \
-DCMAKE_CXX_COMPILER=clang++ \
-DCMAKE_C_COMPILER=clang \
-DCLICE_ENABLE_TEST=ON \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_CXX_FLAGS="-fno-rtti -fno-exceptions -O3 -g" \
-DLLVM_INSTALL_PATH="./deps/llvm/build-install" \
-DCLICE_LIB_TYPE=STATIC