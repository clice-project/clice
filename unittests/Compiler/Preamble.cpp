#include "Test/Test.h"
#include "Compiler/Preamble.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

namespace {

void EXPECT_BOUNDS(std::vector<llvm::StringRef> marks,
                   llvm::StringRef content,
                   LocationChain chain = LocationChain()) {
    Annotation annotation{content};
    auto bounds = computePreambleBounds(annotation.source());

    ASSERT_EQ(bounds.size(), marks.size(), chain);

    for(std::uint32_t i = 0; i < bounds.size(); i++) {
        EXPECT_EQ(bounds[i], annotation.offset(marks[i]), chain);
    }
}

llvm::StringMap<std::string> scan(llvm::StringRef content) {
    llvm::StringMap<std::string> files;
    std::string current_filename;
    std::string current_content;

    auto save_previous_file = [&]() {
        if(current_filename.empty()) {
            return;
        }

        files.try_emplace(current_filename, llvm::StringRef(current_content).trim());
        current_filename.clear();
        current_content.clear();
    };

    while(!content.empty()) {
        llvm::StringRef line = content.take_front(content.find_first_of("\r\n"));
        content = content.drop_front(line.size());
        if(content.starts_with("\r\n")) {
            content = content.drop_front(2);
        } else if(content.starts_with("\n")) {
            content = content.drop_front(1);
        }

        if(line.starts_with("#[") && line.ends_with("]")) {
            save_previous_file();
            current_filename = line.slice(2, line.size() - 1).str();
        } else if(!current_filename.empty()) {
            current_content += line;
            current_content += '\n';
        }
    }

    save_previous_file();

    return files;
}

void EXPECT_BUILD_PCH(llvm::StringRef command,
                      llvm::StringRef test_contents,
                      LocationChain chain = LocationChain()) {
    auto tmp = fs::createTemporaryFile("clice", "pch");
    ASSERT_TRUE(tmp);
    std::string outPath = std::move(*tmp);

    auto files = scan(test_contents);
    ASSERT_TRUE(files.contains("main.cpp"));
    std::string content = files["main.cpp"];
    files.erase("main.cpp");

    CompilationParams params;
    params.srcPath = "main.cpp";
    params.content = content;
    params.outPath = outPath;
    params.bound = computePreambleBound(content);
    params.command = command;

    for(auto& [path, content]: files) {
        params.addRemappedFile(path::join(".", path), content);
    }

    /// Build PCH.
    PCHInfo info;
    {
        /// NOTE: PCH file is written when CompilerInstance is destructed.
        auto AST = compile(params, info);
        ASSERT_TRUE(AST, chain);

        EXPECT_EQ(info.path, outPath, chain);
        EXPECT_EQ(info.command, command, chain);
        /// TODO: EXPECT_EQ(info.deps, deps);
    }

    /// Build AST with PCH.
    for(auto& [path, content]: files) {
        params.addRemappedFile(path::join(".", path), content);
    }

    params.bound.reset();
    params.pch = {info.path, info.preamble.size()};
    auto AST = compile(params);
    ASSERT_TRUE(AST, chain);
}

TEST(Preamble, Bounds) {
    EXPECT_BOUNDS({"0"}, "#include <iostream>$(0)");
    EXPECT_BOUNDS({"0"}, "#include <iostream>$(0)\n");

    EXPECT_BOUNDS({"0", "1"},
                  R"cpp(
#ifdef TEST
#include <iostream>$(0)
#define 1
#endif$(1)
)cpp");

    EXPECT_BOUNDS({"0"},
                  R"cpp(
#include <iostream>$(0)
int x = 1;
)cpp");

    EXPECT_BOUNDS({"0"}, R"cpp(
module;
#include <iostream>$(0)
export module test;
)cpp");
}

TEST(Preamble, TranslationUnit) {
    EXPECT_BUILD_PCH("clang++ -std=c++20 main.cpp",
                     R"cpp(
#[test.h]
int foo();

#[main.cpp]
#include "test.h"
int x = foo();
)cpp");
}

TEST(Preamble, Module) {
    EXPECT_BUILD_PCH("clang++ -std=c++20 main.cpp",
                     R"cpp(
#[test.h]
int foo();

#[main.cpp]
module;
#include "test.h"
export module test;
export int x = foo();
)cpp");
}

TEST(Preamble, Header) {
    llvm::StringRef command = "clang++ -std=c++20 main.cpp";

    llvm::StringRef content = R"cpp(
#[test.h]
int foo();
int bar();

#[test1.h]
#include "test.h"
Point x = {foo(), bar()};

#[test2.h]
struct Point {
    int x;
    int y;
};

#include "test1.h"

#[test3.h]
#include "test2.h"

#[main.cpp]
#include "test3.h"
)cpp";
}

/// FIXME: headers not found
///
/// TEST(Preamble, BuildChainedPreamble) {
///     llvm::StringRef content = R"(
/// #include <cstdio>
/// )";
///
///     CompilationParams params;
///     params.srcPath = "main.pch";
///     params.content = content;
///     params.command = "clang++ -std=c++20 -xc++ main.pch";
///     params.outPath = path::join(".", "header1.pch");
///     params.bound = computePreambleBound(content);
///
///     {
///         PCHInfo out;
///         auto AST = compile(params, out);
///         if(!AST) {
///             println("error: {}", AST.error());
///         }
///         llvm::outs() << "bound: " << *params.bound << "\n";
///     }
///
///     content = R"(
/// #include <cstdio>
/// #include <cmath>
/// )";
///
///     params.pch = std::pair{params.outPath.str(), *params.bound};
///     params.content = content;
///     params.outPath = path::join(".", "header2.pch");
///     params.bound = computePreambleBound(content);
///
///     {
///         PCHInfo out;
///         auto AST = compile(params, out);
///         if(!AST) {
///             println("error: {}", AST.error());
///         }
///         llvm::outs() << "bound: " << *params.bound << "\n";
///     }
///
///     content = R"(
/// int main() {
///     auto y = abs(1.0);
///     return 0;
/// }
/// )";
///
///     params.pch = std::pair{params.outPath.str(), 0};
///     params.srcPath = "main.cpp";
///     params.command = "clang++ -std=c++20 main.cpp";
///     params.content = content;
///     params.outPath = path::join(".", "header2.pch");
///
///     {
///         auto AST = compile(params);
///         if(!AST) {
///             println("error: {}", AST.error());
///         }
///         llvm::outs() << "bound: " << *params.bound << "\n";
///         /// AST->tu()->dump();
///     }
/// }

}  // namespace

}  // namespace clice::testing

