#include <Test/Test.h>
#include <Index/Indexer.h>
#include <Compiler/Compiler.h>
#include <Test/Index.h>

namespace {

using namespace clice;

bool TestGotoDefinition(int a, int b) {
    llvm::outs() << "TestGotoDefinition\n";
    llvm::outs() << a << " " << b << "\n";
    return true;
}

REGISTER(test::GotoDefinition, TestGotoDefinition)

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

TEST(clice, Index) {
    foreachFile("Index", [](llvm::StringRef filepath, llvm::StringRef content) {
        Compiler compiler("main.cpp", content, compileArgs);
        compiler.buildAST();
        Indexer slab(compiler.sema(), compiler.tokBuf());
        auto csif = slab.index();
        auto value = json::serialize(csif);
        std::error_code EC;
        llvm::raw_fd_ostream fileStream("output.json", EC);
        fileStream << value << "\n";
        llvm::outs() << value << "\n";
    });
}

TEST(clice, GotoDefinition) {
    const char* code = R"(
namespace test {

template <typename... Ts>
struct Hook {
    consteval Hook() {}

    consteval Hook(Ts... args) {}
};

using GotoDefinition = Hook<int, int>;
}

namespace Cases {

test::GotoDefinition case1(100, 2);

};
    )";
    REGISTER(test::GotoDefinition, TestGotoDefinition);
    Compiler compiler("main.cpp", code, compileArgs);
    compiler.buildAST();
    auto& context = compiler.context();
    exec(context);
}

}  // namespace

