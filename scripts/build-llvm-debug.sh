cmake \
    -G Ninja -S ./llvm \
    -B build-debug \
    -DLLVM_USE_LINKER=lld \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_USE_SANITIZER=Address \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_INSTALL_PREFIX=./build-debug-install
cmake --build build-debug
cmake --install build-debug

src="./clang/lib/Sema/"
dst="./build-debug-install/include/clang/Sema/"

files=("CoroutineStmtBuilder.h" "TypeLocBuilder.h" "TreeTransform.h")

for file in "${files[@]}"; do
    echo "Copying ${src}${file} to ${dst}${file}"
    cp "${src}${file}" "${dst}${file}"
done
