#include <gtest/gtest.h>
#include <Support/FileSystem.h>
#include <Index/Serialize.h>
#include <Compiler/Compiler.h>

#include "../Test.h"

namespace {

using namespace clice;

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
        mainFileIndex = mainFile();
    }

    uint32_t mainFile() {
        for(uint32_t i = 0; i < index.files.size(); i++) {
            if(!index.files[i].include.isValid()) {
                return i;
            }
        }

        std::terminate();
    }

    IndexTester& GoToDefinition(index::Position source, index::Position target) {
        auto occurrence = std::lower_bound(index.occurrences.begin(),
                                           index.occurrences.end(),
                                           source,
                                           [](const auto& occurrence, index::Position pos) {
                                               return occurrence.location.end < pos;
                                           });

        EXPECT_TRUE(occurrence != index.occurrences.end());
        EXPECT_TRUE(occurrence->location.begin <= source);
        auto& id = occurrence->target;

        auto symbol = std::lower_bound(index.symbols.begin(),
                                       index.symbols.end(),
                                       id,
                                       [](const auto& symbol, const auto& id) {
                                           return refl::less(symbol.id, id);
                                       });

        EXPECT_TRUE(symbol != index.symbols.end());
        EXPECT_TRUE(refl::equal(symbol->id, id));

        bool hasDefinition = false;
        for(auto& relation: symbol->relations) {
            if(relation.kind.is(index::RelationKind::Definition)) {
                auto location = relation.location;
                hasDefinition = true;
                EXPECT_EQ(location.begin, target);
            }
        }

        EXPECT_TRUE(hasDefinition);

        return *this;
    }

    uint32_t mainFileIndex;

    std::string filepath;
    std::vector<const char*> args;
    std::unique_ptr<Compiler> compiler;
    index::memory::Index index;
};

// TEST(Index, index) {
//     foreachFile("Index", [](std::string filepath, llvm::StringRef content) {
//         std::vector<const char*> compileArgs = {
//             "clang++",
//             "-std=c++20",
//             filepath.c_str(),
//             "-resource-dir",
//             "/home/ykiko/C++/clice2/build/lib/clang/20",
//         };
//         if(filepath.ends_with("MemberExpr.cpp")) {
//             Compiler compiler(compileArgs);
//             compiler.buildAST();
//
//             auto index = index::index(compiler);
//             llvm::outs() << "files count:" << index.files.size() << "\n";
//             auto json = index::toJson(index);
//             // llvm::outs() << json << "\n";
//
//             std::error_code error;
//             llvm::raw_fd_ostream file("index.json", error);
//             file << json;
//             // auto file = index.files[0];
//             // while(file.include.isValid()) {
//             //     auto includeLoc = file.include;
//             //     llvm::outs() << index.files[includeLoc.file].path << ":" <<
//             includeLoc.begin.line
//             //     << ":" << includeLoc.begin.column << "\n"; file =
//             index.files[file.include.file];
//             // }
//             // llvm::outs() << file.path << "\n";
//         }
//     });
// }

TEST(Index, Expr) {
    IndexTester tester("Expr.cpp");
    // tester.GoToDefinition(index::Position(9, 14), index::Position(4, 9));
    tester.compiler->tu()->dump();
}

}  // namespace

