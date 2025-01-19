#include "Test/Test.h"
#include "Server/Indexer.h"

namespace clice::testing {

TEST(Indexer, Basic) {
    config::IndexOptions options;
    options.dir = path::join(".", "temp");

    CompilationDatabase database;
    auto prefix = path::join(test_dir(), "indexer");
    auto foo = path::join(prefix, "foo.cpp");
    auto main = path::join(prefix, "main.cpp");
    database.updateCommand(foo, std::format("clang++ {}", foo));
    database.updateCommand(main, std::format("clang++ {}", main));

    Indexer indexer(options, database);
    auto p1 = indexer.index(main);
    auto p2 = indexer.index(foo);
    async::schedule(p1.release());
    async::schedule(p2.release());

    async::run();

    indexer.saveToDisk();
    
}

}  // namespace clice::testing
