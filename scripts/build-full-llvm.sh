cmake \
    -G Ninja -S ./llvm \
    -B build-llvm \
    -DLLVM_USE_LINKER=lld \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DLLVM_ENABLE_RUNTIMES=compiler-rt
cmake --build build-llvm
cmake --install build-llvm
