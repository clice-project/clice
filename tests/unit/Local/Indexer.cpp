#include "Test/Test.h"
#include "Server/Indexer.h"
#include "Index/SymbolIndex.h"
#include "Support/Binary.h"

namespace clice::testing {

void foo(int x, int y = 2) {}

#define self(x) x
#define call(x) foo(x)
#define call2(x) foo(x, x)
#define call3 foo(1, 2)

void test() {
    /// caller and argument have same expansion location, but
    /// have different spelling location and in macro arguments.
    self(foo(1));
    self(foo)(1);
    self(foo)(self(1));
    call(1);
    call2(2);
    call3;
}

TEST(Indexer, Assert) {

    // config::IndexOptions options;
    // options.dir = path::join(".", "temp");
    // auto error = fs::create_directories(options.dir);
    //
    // CompilationDatabase database;
    // auto prefix = path::join(test_dir(), "indexer");
    // auto main = path::real_path(path::join(prefix, "assert.cpp"));
    // database.updateCommand(main, std::format("clang++ {}", main));
    //
    // Indexer indexer(options, database);
    // indexer.loadFromDisk();
    //
    // auto p1 = indexer.index(main);
    // async::run(p1);
    //
    // indexer.saveToDisk();
}

TEST(Indexer, IndexAll) {
    // config::IndexOptions options;
    // options.dir = path::join(".", "temp");
    // auto error = fs::create_directories(options.dir);
    //
    // CompilationDatabase database;
    // database.updateCommands("/home/ykiko/C++/clice/build/compile_commands.json");
    //
    // Indexer indexer(database, options);
    // // indexer.load();
    //
    // /// indexer.indexAll();
    // // indexer.indexAll();
    // /// async::run();
    //
    // indexer.save();
}

template <typename T, int ID>
struct AnnoatedVar {
    T value;
};

TEST(Indexer, Debug) {

    // config::IndexOptions options;
    // options.dir = path::join(".", "temp");
    // auto error = fs::create_directories(options.dir);
    //
    // CompilationDatabase database;
    // database.updateCommands("/home/ykiko/C++/clice/build/compile_commands.json");
    //
    // Indexer indexer(options, database);
    // indexer.loadFromDisk();
    //
    // indexer.dumpForTest("/usr/include/assert.h");
    // indexer.saveToDisk();
}

template <typename Object, typename B = binary::binarify_t<Object>>
bool compareDifference(binary::Proxy<Object> lhs, binary::Proxy<Object> rhs) {
    if constexpr(binary::is_directly_binarizable_v<Object>) {
        if(!refl::equal(lhs.value(), rhs.value())) {
            println("lhs: {}, rhs: {}", dump(lhs.value()), dump(rhs.value()));
            return false;
        }
        return true;
    } else if constexpr(std::is_same_v<Object, std::string>) {
        if(lhs.as_string() != rhs.as_string()) {
            println("lhs: {}, rhs: {}", lhs.as_string(), rhs.as_string());
            return false;
        }
        return true;
    } else if constexpr(requires { typename Object::value_type; }) {
        bool result = true;
        if(lhs.size() != rhs.size()) {
            println("lhs: {}, rhs: {}", lhs.size(), rhs.size());
            result = false;
        }

        for(std::size_t i = 0; i < lhs.size(); i++) {
            if(!compareDifference<typename Object::value_type>(lhs[i], rhs[i])) {
                result = false;
            }
        }

        return result;
    } else if constexpr(refl::reflectable_struct<Object>) {
        bool same = true;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((same &= compareDifference(lhs.template get<Is>(), rhs.template get<Is>())), ...);
        }(std::make_index_sequence<refl::member_count<Object>()>{});
        return same;
    }
}

TEST(Indexer, Binary) {
    // auto file1 =
    //     llvm::MemoryBuffer::getFile("/home/ykiko/C++/clice/temp/StringRef.h.1738427457313.sidx");
    // auto file2 =
    //     llvm::MemoryBuffer::getFile("/home/ykiko/C++/clice/temp/StringRef.h.1738427518359.sidx");
    //
    // auto buffer1 = file1.get()->getBuffer();
    // auto buffer2 = file2.get()->getBuffer();
    //
    // println("size1: {}, size2: {}", buffer1.size(), buffer2.size());
    // println("binary same: {}", buffer1 == buffer2);
    //
    // binary::Proxy<index::memory::SymbolIndex> p1{const_cast<char*>(buffer1.data()),
    //                                             const_cast<char*>(buffer1.data())};
    //
    // binary::Proxy<index::memory::SymbolIndex> p2{const_cast<char*>(buffer2.data()),
    //                                             const_cast<char*>(buffer2.data())};
    //
    // bool v = compareDifference<index::memory::SymbolIndex>(p1, p2);
    // println("binary difference: {}", v);
    //
    // auto r1 = p1.get<"ranges">().as_array();
    // auto r2 = p2.get<"ranges">().as_array();
    //
    // for(std::size_t i = 194; i < 199; i++) {
    //    println("offset: {}, range1: {}", i, dump(r1[i]));
    //}
    //
    // for(std::size_t i = 0; i < r1.size(); i++) {
    //    auto range1 = r1[i];
    //    auto range2 = r2[i];
    //
    //    if(!refl::equal(range1, range2)) {
    //        println("offset: {}, range1: {}, range2: {}", i, dump(range1), dump(range2));
    //    }
    //}
    //
    // auto s1 = index::SymbolIndex{const_cast<char*>(buffer1.data()), buffer1.size(), false};
    // auto s2 = index::SymbolIndex{const_cast<char*>(buffer2.data()), buffer2.size(), false};
    //
    // println("json same: {}", s1.toJSON() == s2.toJSON());
}

void dump(clang::Token& token) {
    println("kind: {}, identifier: {}",
            token.getName(),
            token.is(clang::tok::raw_identifier) ? token.getRawIdentifier() : "");
}

}  // namespace clice::testing
