#include "../Test.h"

#include "Compiler/Command.h"

namespace clice {

namespace {

TEST(clice, Command) {
    llvm::SmallString<1024> buffer;
    llvm::SmallVector<const char*, 16> args;

    auto command =
        "/usr/local/bin/clang++" 
        " -Dclice_core_EXPORTS"
        " -I/home/ykiko/C++/clice2/deps/llvm/build-install/include"
        " -I/home/ykiko/C++/clice2/include"
        " -I/home/ykiko/C++/clice2/deps/toml/include"
        " -I/home/ykiko/C++/clice2/deps/libuv/include"
        "  -fno-rtti"
        " -fno-exceptions"
        " -g -O0 -fsanitize=address"
        " -Wno-deprecated-declarations"
        " -g -std=gnu++23 -fPIC -Winvalid-pch"
        " -Xclang -include-pch"
        " -Xclang /home/ykiko/C++/clice2/build/CMakeFiles/clice-core.dir/cmake_pch.hxx.pch"
        " -Xclang -include -Xclang /home/ykiko/C++/clice2/build/CMakeFiles/clice-core.dir/cmake_pch.hxx"
        " -o CMakeFiles/clice-core.dir/src/Basic/URI.cpp.o"
        " -c /home/ykiko/C++/clice2/src/Basic/URI.cpp";

    auto error = mangleCommand(command, args, buffer);
    ASSERT_FALSE(bool(error));
}

}  // namespace

}  // namespace clice
