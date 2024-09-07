cd deps/llvm
cmake -B build -S ./llvm -G Ninja \
-DCMAKE_C_COMPILER=clang \
-DCMAKE_CXX_COMPILER=clang++ \
-DLLVM_USE_LINKER=lld \
-DCMAKE_BUILD_TYPE=Debug \
-DLLVM_TARGETS_TO_BUILD="X86" \
-DLLVM_ENABLE_PROJECTS="clang" \
-DBUILD_SHARED_LIBS=ON \
-DCMAKE_INSTALL_PREFIX=./build-install

cmake --build build
cmake --build build --target install













