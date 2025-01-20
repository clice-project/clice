import os
import subprocess
import shutil

# CMake build commands
cmake_commands = [
    ["cmake", "-G", "Ninja", "-S", "./llvm", "-B", "build-debug",
     "-DLLVM_USE_LINKER=lld", "-DCMAKE_C_COMPILER=clang",
     "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug",
     "-DBUILD_SHARED_LIBS=ON", "-DLLVM_TARGETS_TO_BUILD=X86",
     "-DLLVM_USE_SANITIZER=Address", "-DLLVM_ENABLE_PROJECTS=clang",
     "-DCMAKE_INSTALL_PREFIX=./build-debug-install"],
    ["cmake", "--build", "build-debug"],
    ["cmake", "--install", "build-debug"]
]

# Execute CMake commands
for command in cmake_commands:
    subprocess.run(command, check=True)

# File copy section
src = "./clang/lib/Sema/"
dst = "./build-debug-install/include/clang/Sema/"
files = ["CoroutineStmtBuilder.h", "TypeLocBuilder.h", "TreeTransform.h"]

for file in files:
    src_file = os.path.join(src, file)
    dst_file = os.path.join(dst, file)
    print(f"Copying {src_file} to {dst_file}")
    shutil.copy(src_file, dst_file)
