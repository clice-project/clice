#include "Test/Test.h"
#include "Compiler/Preamble.h"
#include "Compiler/Compilation.h"
#include "Test/Annotation.h"

namespace clice::testing {

namespace {

void EXPECT_BOUNDS(std::vector<llvm::StringRef> marks,
                   llvm::StringRef content,
                   LocationChain chain = LocationChain()) {
    auto annotation = AnnotatedSource::from(content);

    auto bounds = computePreambleBounds(annotation.content);

    ASSERT_EQ(bounds.size(), marks.size(), chain);

    for(std::uint32_t i = 0; i < bounds.size(); i++) {
        EXPECT_EQ(bounds[i], annotation.offsets[marks[i]], chain);
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

void EXPECT_BUILD_PCH(llvm::StringRef main_file,
                      llvm::StringRef test_contents,
                      llvm::StringRef preamble = "",
                      LocationChain chain = LocationChain()) {
    auto tmp = fs::createTemporaryFile("clice", "pch");
    ASSERT_TRUE(tmp);
    std::string outPath = std::move(*tmp);

    auto files = scan(test_contents);
    if(!preamble.empty()) {
        files.try_emplace("preamble.h", preamble);
    }

    ASSERT_TRUE(files.contains(main_file));
    std::string content = files[main_file];
    files.erase(main_file);

    CompilationParams params;
    params.output_file = outPath;
    auto bound = computePreambleBound(content);
    params.add_remapped_file(main_file, content, bound);

    params.arguments = {
        "clang++",
        "-xc++",
        "-std=c++20",
    };

    if(!preamble.empty()) {
        params.arguments.emplace_back("--include=preamble.h");
    }

    std::string buffer = main_file.str();
    params.arguments.emplace_back(buffer.c_str());

    for(auto& [path, content]: files) {
        params.add_remapped_file(path::join(".", path), content);
    }

    /// Build PCH.
    PCHInfo info;
    {
        /// NOTE: PCH file is written when CompilerInstance is destructed.
        auto AST = compile(params, info);
        ASSERT_TRUE(AST, chain);

        EXPECT_EQ(info.path, outPath, chain);
        /// EXPECT_EQ(info.command, params.arguments, chain);
        /// TODO: EXPECT_EQ(info.deps, deps);
    }

    /// Build AST with PCH.
    for(auto& [path, content]: files) {
        params.add_remapped_file(path::join(".", path), content);
    }

    params.add_remapped_file(main_file, content);
    params.pch = {info.path, info.preamble.size()};
    auto unit = compile(params);
    ASSERT_TRUE(unit, chain);
}

TEST(Preamble, Bounds) {
    EXPECT_BOUNDS({}, "int main(){}");

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
    EXPECT_BUILD_PCH("main.cpp",
                     R"cpp(
#[test.h]
int foo();

#[main.cpp]
#include "test.h"
int x = foo();
)cpp");
}

TEST(Preamble, Module) {
    EXPECT_BUILD_PCH("main.cpp",
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
    llvm::StringRef test_contents = R"cpp(
#[test.h]
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
int foo();

#[main.cpp]
#include "test3.h"
#include "test2.h"
)cpp";

    auto files = scan(test_contents);
    ASSERT_TRUE(files.contains("main.cpp"));
    std::string content = files["main.cpp"];
    files.erase("main.cpp");

    std::string preamble;

    /// Compute implicit include.
    {

        CompilationParams params;
        params.add_remapped_file("main.cpp", content);
        params.arguments = {"clang++", "-std=c++20", "main.cpp"};

        for(auto& [path, file]: files) {
            params.add_remapped_file(path::join(".", path), file);
        }

        auto unit = preprocess(params);
        ASSERT_TRUE(unit);

        auto path = path::join(".", "test1.h");
        auto fid = unit->file_id(path);
        ASSERT_TRUE(fid.isValid());

        while(fid.isValid()) {
            auto location = unit->include_location(fid);
            auto [fid2, offset] = unit->decompose_location(location);
            auto content = unit->file_content(fid2).substr(0, offset);

            /// Remove incomplete include.
            content = content.substr(0, content.rfind("\n"));
            preamble += content;
            fid = fid2;
        }
    }

    EXPECT_BUILD_PCH("test1.h", test_contents, preamble);
}

TEST(Preamble, Chain) {
    llvm::StringRef test_contents = R"cpp(
#[test.h]
int bar();

#[test2.h]
int foo();

#[main.cpp]
#include "test.h"
#include "test2.h"
int x = bar();
int y = foo();
)cpp";

    auto files = scan(test_contents);
    ASSERT_TRUE(files.contains("main.cpp"));
    std::string content = files["main.cpp"];
    files.erase("main.cpp");

    auto bounds = computePreambleBounds(content);

    CompilationParams params;
    params.arguments = {"clang++", "-std=c++20", "main.cpp"};

    PCHInfo info;
    std::uint32_t last_bound = 0;
    for(auto bound: bounds) {
        auto tmp = fs::createTemporaryFile("clice", "pch");
        ASSERT_TRUE(tmp);
        std::string outPath = std::move(*tmp);

        params.add_remapped_file("main.cpp", content, bound);
        if(params.output_file.empty()) {
            params.pch = {params.output_file.str().str(), last_bound};
        }

        params.output_file = outPath;
        last_bound = bound;

        for(auto& [path, content]: files) {
            params.add_remapped_file(path::join(".", path), content);
        }

        {
            auto AST = compile(params, info);
            ASSERT_TRUE(AST);

            EXPECT_EQ(info.path, outPath);
            /// EXPECT_EQ(info.command, params.arguments);
        }
    }

    /// Build AST with PCH.
    for(auto& [path, content]: files) {
        params.add_remapped_file(path::join(".", path), content);
    }

    params.add_remapped_file("main.cpp", content);
    params.pch = {info.path, last_bound};
    auto unit = compile(params);
    ASSERT_TRUE(unit);
}

}  // namespace

}  // namespace clice::testing

