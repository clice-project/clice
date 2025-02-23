#include "Test/Test.h"

#include "Server/Database.h"

namespace clice::testing {

namespace {

void check(llvm::StringRef jsonText) {
    auto object = json::parse(llvm::StringRef(jsonText));
    EXPECT_TRUE(bool(object));

    auto res = parseCompileCommand(object->getAsObject());
    EXPECT_TRUE(res.has_value());

    if(!res.has_value()) {
        llvm::outs() << res.error();
    }

    auto [file, command] = std::move(res).value();
    llvm::StringRef expectedFile = "/home/shiyu/github/clice/src/Driver/clice.cc";
    EXPECT_EQ(file, expectedFile);
    llvm::outs() << command << '\n';
}

TEST(CompilationDatabase, Command) {
    auto cmake = R"json(
{
  "directory": "/home/shiyu/github/clice/build",
  "command": "/usr/bin/clang++-20 -I/home/shiyu/github/clice/./.llvm/include -I/home/shiyu/github/clice/include -I/home/shiyu/github/clice/build/_deps/libuv-src/include -isystem /home/shiyu/github/clice/build/_deps/tomlplusplus-src/include  -fno-rtti -fno-exceptions -g -O0 -fsanitize=address -Wno-deprecated-declarations -g -std=gnu++23 -o CMakeFiles/clice.dir/src/Driver/clice.cc.o -c /home/shiyu/github/clice/src/Driver/clice.cc",
  "file": "/home/shiyu/github/clice/src/Driver/clice.cc",
  "output": "CMakeFiles/clice.dir/src/Driver/clice.cc.o"
}
  )json";

    auto xmake = R"json(
{
  "directory": "/home/shiyu/github/clice",
  "arguments": ["/usr/bin/clang", "-c", "-Qunused-arguments", "-m64", "-g", "-O0", "-std=c++23", "-Iinclude", "-fno-exceptions", "-fno-cxx-exceptions", "-isystem", "/home/shiyu/.xmake/packages/l/libuv/v1.49.2/5ba3a0ddfd5e4448beb78c29cbfeaaa4/include", "-isystem", "/home/shiyu/.xmake/packages/t/toml++/v3.4.0/bde7344d843e41928b1d325fe55450e0/include", "-isystem", "/home/shiyu/.xmake/packages/l/llvm/20.0.0/40421d4ceadb44b49ffc6cb766f3722a/include", "-fsanitize=address", "-fno-rtti", "-o", "build/.objs/clice/linux/x86_64/debug/src/Driver/clice.cc.o", "src/Driver/clice.cc"],
  "file": "src/Driver/clice.cc"
}
  )json";

    // check(cmake);
    // check(xmake);
}

TEST(CompilationDatabase, Module) {}

}  // namespace

}  // namespace clice::testing
