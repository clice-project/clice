#include <gtest/gtest.h>
#include <Support/FileSystem.h>
#include <Index/Serialize.h>
#include <Compiler/Compiler.h>
#include <Index/Loader.h>
#include "Annotation.h"
#include "../Test.h"

namespace {

using namespace clice;

using namespace clice::index;

template <typename In, typename Out>
void test_equal(const In& in, const Out& out, const char* data) {
    if constexpr(requires { in == out; }) {
        EXPECT_TRUE(in == out);
    } else if constexpr(std::is_same_v<Out, binary::string>) {
        EXPECT_EQ(in, llvm::StringRef(data + out.offset, out.size).str());
    } else if constexpr(is_specialization_of<Out, binary::array>) {
        using value_type = typename Out::value_type;
        auto array =
            llvm::ArrayRef(reinterpret_cast<const value_type*>(data + out.offset), out.size);
        for(std::size_t i = 0; i < in.size(); i++) {
            test_equal(in[i], array[i], data);
        }
    } else {
        refl::foreach(in, out, [data](const auto& lhs, const auto& rhs) {
            test_equal(lhs, rhs, data);
        });
    }
}

struct IndexTester {
    IndexTester(llvm::StringRef filepath) {
        llvm::SmallString<128> path;
        path::append(path, test_dir(), "Index", filepath);
        this->filepath = path.str();
        args = {
            "clang++",
            "-std=c++20",
            this->filepath.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };
        compiler = std::make_unique<Compiler>(args);
        compiler->buildAST();
        index = index::index(*compiler);
        // mainFileIndex = mainFile();
    }

    std::string filepath;
    std::vector<const char*> args;
    std::unique_ptr<Compiler> compiler;
    index::memory::Index index;
};

TEST(Index, Annotation) {
    Annotation annotation(R"cpp(
int @name = 1;

int main() {
    $(d1)name = 2;
}
    )cpp");

    llvm::outs() << annotation.source() << "\n";
    auto pos = annotation.position("name");
    llvm::outs() << pos.line << ", " << pos.column << "\n";
    pos = annotation.position("d1");
    llvm::outs() << pos.line << ", " << pos.column << "\n";

    std::vector<const char*> args = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    llvm::outs() << annotation.source() << "\n";

    Compiler compiler("main.cpp", annotation.source(), args);
    compiler.buildAST();
    compiler.tu()->dump();
    auto index = index::index(compiler);

    auto binary = index::toBinary(index);
    auto loader = Loader(std::move(binary));

    auto json = index::toJson(index);
    std::error_code error;
    llvm::raw_fd_ostream file("index.json", error);
    file << json;

    auto files = loader.locateFile("main.cpp");
    assert(files.size() == 1);
    FileRef id = {static_cast<uint32_t>(files.begin() - loader.files().begin())};
    auto sym = loader.locateSymbol(id, annotation.position("name"));
    loader.lookupRelation(sym, RelationKind::Reference, [&](const FullLocation& location) {
        llvm::outs() << location.filepath << ": " << location.begin.line << ", "
                     << location.begin.column << "\n";
    });
}

}  // namespace

