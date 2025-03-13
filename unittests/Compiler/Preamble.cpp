#include "Test/Test.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Preamble.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

namespace {

TEST(Preamble, ComputePreambleBound) {
    proto::Position pos;
    SourceConverter converter;

    Annotation annotation = {"\n\n\nint x = 1;"};

    auto compute = [&](llvm::StringRef source) {
        annotation = {source};
        auto content = annotation.source();
        return converter.toPosition(content, computePreambleBound(content));
    };

    pos = compute("#include <iostream>$(end)");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute("#include <iostream>$(end)\n");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
#ifdef TEST
#include <iostream>
#define 1
#endif$(end)
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
#include <iostream>$(end)
int x = 1;
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
module;
#include <iostream>$(end)
export module test;
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));
}

TEST(Preamble, BuildPreambleForTU) {
    auto outPath = path::join(".", "main.pch");
    llvm::StringRef command = "clang++ -std=c++20 main.cpp";

    llvm::StringRef test = R"cpp(
int foo();
)cpp";

    llvm::StringRef content = R"cpp(#include "test.h"
int x = foo();
)cpp";

    std::vector<std::string> deps = {path::join(".", "test.h")};

    CompilationParams params;
    params.outPath = outPath;
    params.srcPath = "main.cpp";
    params.content = content;
    params.command = command;
    params.bound = computePreambleBound(content);

    llvm::SmallString<128> path;
    params.remappedFiles.emplace_back(deps[0], test);

    /// Build PCH.
    PCHInfo out;
    {
        auto info = compile(params, out);
        EXPECT_TRUE(bool(info));

        EXPECT_EQ(out.path, outPath);
        EXPECT_EQ(out.preamble, R"(#include "test.h")");
        EXPECT_EQ(out.command, command);
        EXPECT_EQ(out.deps, deps);
    }

    /// Build AST
    {
        params.bound.reset();
        params.pch = {outPath, out.preamble.size()};
        auto info = compile(params);
        EXPECT_TRUE(bool(info));
    }
}

TEST(Preamble, BuildChainedPreamble) {
    llvm::StringRef content = R"(
#include <cstdio>
)";

    CompilationParams params;
    params.srcPath = "main.pch";
    params.content = content;
    params.command = "clang++ -std=c++20 -xc++ main.pch";
    params.outPath = path::join(".", "header1.pch");
    params.bound = computePreambleBound(content);

    {
        PCHInfo out;
        auto AST = compile(params, out);
        if(!AST) {
            println("error: {}", AST.error());
        }
        llvm::outs() << "bound: " << *params.bound << "\n";
    }

    content = R"(
#include <cstdio>
#include <cmath>
)";

    params.pch = std::pair{params.outPath.str(), *params.bound};
    params.content = content;
    params.outPath = path::join(".", "header2.pch");
    params.bound = computePreambleBound(content);

    {
        PCHInfo out;
        auto AST = compile(params, out);
        if(!AST) {
            println("error: {}", AST.error());
        }
        llvm::outs() << "bound: " << *params.bound << "\n";
    }

    content = R"(
int main() {
    auto y = abs(1.0);
    return 0;
}
)";

    params.pch = std::pair{params.outPath.str(), 0};
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";
    params.content = content;
    params.outPath = path::join(".", "header2.pch");
    params.bound.reset();

    {
        auto AST = compile(params);
        if(!AST) {
            println("error: {}", AST.error());
        }
        llvm::outs() << "bound: " << *params.bound << "\n";
        /// AST->tu()->dump();
    }
}

TEST(Preamble, BuildPreambleForHeader) {
    /// TODO: The key point is find interested file according to the header context.
}

TEST(Preamble, BuildPreambleForMU) {
    auto outPath = path::join(".", "main.pch");
    llvm::StringRef command = "clang++ -std=c++20 main.cpp";

    llvm::StringRef test = R"cpp(
int foo();
)cpp";

    llvm::StringRef content = R"cpp(
module;
#include "test.h"
export module test;
export int x = foo();
)cpp";

    std::vector<std::string> deps = {path::join(".", "test.h")};

    CompilationParams params;
    params.outPath = outPath;
    params.srcPath = "main.cpp";
    params.content = content;
    params.command = command;
    params.bound = computePreambleBound(content);

    llvm::SmallString<128> path;
    params.remappedFiles.emplace_back(deps[0], test);

    /// Build PCH.
    PCHInfo out;
    {
        auto info = compile(params, out);
        EXPECT_TRUE(bool(info));

        EXPECT_EQ(out.path, outPath);
        EXPECT_EQ(out.preamble, llvm::StringRef(R"(
module;
#include "test.h")"));
        EXPECT_EQ(out.command, command);
        EXPECT_EQ(out.deps, deps);
    }

    /// Build AST
    {
        params.bound.reset();
        params.pch = {outPath, out.preamble.size()};
        auto info = compile(params);
        EXPECT_TRUE(bool(info));
    }
}

}  // namespace

}  // namespace clice::testing

