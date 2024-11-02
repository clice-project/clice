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
        // mainFileIndex = mainFile();
    }

    uint32_t mainFile() {
        for(uint32_t i = 0; i < index.files.size(); i++) {
            if(!index.locations[index.files[i].include].isValid()) {
                return i;
            }
        }

        std::terminate();
    }

    auto& findSymbol(index::Position source) {
        auto occurrence =
            std::lower_bound(index.occurrences.begin(),
                             index.occurrences.end(),
                             source,
                             [&](index::Occurrence occurrence, index::Position pos) {
                                 return index.locations[occurrence.location].end < pos;
                             });

        EXPECT_TRUE(occurrence != index.occurrences.end());
        EXPECT_TRUE(index.locations[occurrence->location].begin <= source);
        auto& symbol = index.symbols[occurrence->symbol];

        // auto symbol = std::lower_bound(index.symbols.begin(),
        //                                index.symbols.end(),
        //                                id,
        //                                [](const auto& symbol, const auto& id) {
        //                                    return refl::less(symbol.id, id);
        //                                });

        // EXPECT_TRUE(symbol != index.symbols.end());
        // EXPECT_TRUE(refl::equal(symbol->id, id));

        return symbol;
    }

    IndexTester& GoToDefinition(index::Position source, index::Position target) {
        auto& symbol = findSymbol(source);

        bool hasDefinition = false;
        for(auto& relation: symbol.relations) {
            if(relation.kind.is(index::RelationKind::Definition)) {
                auto location = relation.location;
                hasDefinition = true;
                EXPECT_EQ(index.locations[location].begin, target);
            }
        }

        EXPECT_TRUE(hasDefinition);

        return *this;
    }

    IndexTester& CallRelation(index::Position source, index::Position target) {
        auto& symbol = findSymbol(source);

        bool hasCall = false;
        for(auto& relation: symbol.relations) {
            if(relation.kind.is(index::RelationKind::Callee)) {
                auto location = relation.location;
                hasCall = true;
                // EXPECT_EQ(location.begin, target);
                llvm::outs() << json::serialize(relation) << "\n";
            }

            if(relation.kind.is(index::RelationKind::Caller)) {
                auto location = relation.location;
                hasCall = true;
                // EXPECT_EQ(location.begin, target);
            }
        }

        EXPECT_TRUE(hasCall);

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

TEST(Index, Macro) {
    IndexTester tester("Macro.cpp");
    // tester.GoToDefinition(index::Position(9, 14), index::Position(4, 9));
    tester.compiler->tu()->dump();
    auto& srcMgr = tester.compiler->srcMgr();
    for(auto tok: tester.compiler->tokBuf().expandedTokens()) {
        if(tok.location().isMacroID()) {
            llvm::outs() << tok.text(srcMgr) << " ";
            llvm::outs() << srcMgr.getPresumedLineNumber(tok.location()) << ":"
                         << srcMgr.getPresumedColumnNumber(tok.location()) << "\n";
            tok.location().dump(srcMgr);
        }
    }
}

using namespace clice::index;

static_assert(
    is_specialization_of<clice::index::binary::array<clice::index::Location>, binary::array>);

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

TEST(Index, Call) {
    IndexTester tester("Vector.cpp");
    // tester.CallRelation(index::Position(9, 14), index::Position(4, 9));
    // tester.compiler->tu()->dump();

    auto json = index::toJson(tester.index);

    std::error_code error;
    llvm::raw_fd_ostream file("index.json", error);
    file << json;

    llvm::outs() << "symbol count: " << tester.index.symbols.size() << "\n";
    llvm::outs() << "location count: " << tester.index.locations.size() << "\n";
    llvm::outs() << "occurrence count: " << tester.index.occurrences.size() << "\n";

    llvm::outs() << tester.index.symbols.size() * sizeof(binary::Symbol) +
                        tester.index.locations.size() * sizeof(Location) +
                        tester.index.occurrences.size() * sizeof(Occurrence)
                 << "\n";

    std::vector<char> binary = index::toBinary(tester.index);
    char* data = binary.data();
    test_equal(tester.index, *reinterpret_cast<const binary::Index*>(data), data);
    llvm::outs() << binary.size() << "\n";

    llvm::SmallVector<uint8_t, 128> out;
    llvm::compression::zlib::compress(
        llvm::ArrayRef(reinterpret_cast<uint8_t*>(data), binary.size()),
        out);
    llvm::outs() << out.size() << "\n";

    // Decompress
    llvm::SmallVector<uint8_t> decompressedData(out.size());  // Allocate enough space
    auto decompressStart = std::chrono::high_resolution_clock::now();
    std::size_t size = 0;
    llvm::compression::zlib::decompress(out, decompressedData, size);
    auto decompressEnd = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> decompressDuration = decompressEnd - decompressStart;
    llvm::outs() << "Decompression time: " << decompressDuration.count() << " ms\n";
    llvm::outs() << decompressedData.size() << "\n";
}

}  // namespace

