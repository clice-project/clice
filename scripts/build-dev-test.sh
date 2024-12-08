cmake -B build -G Ninja \
-DCMAKE_CXX_COMPILER=clang++ \
-DCMAKE_C_COMPILER=clang \
-DCMAKE_BUILD_TYPE=Debug \
-DCLICE_ENABLE_TEST=ON \
-DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -g -O0 -fsanitize=address -Wno-deprecated-declarations" \
-DCMAKE_LINKER_FLAGS="${CMAKE_LINKER_FLAGS} -fsanitize=address" \
-DLLVM_INSTALL_PATH="./deps/llvm/build-install" \
-DCLICE_LIB_TYPE=SHARED