cmake -B build-runtime -S ./runtimes -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt;libc;libcxx;libcxxabi;libunwind" \
    -DLIBCXX_ENABLE_SHARED=ON \
    -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -w" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_LINKER=lld
cmake --build build-runtime
cmake --build build-runtime --target install
