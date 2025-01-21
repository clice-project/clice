#include "Test/Test.h"
#include "Server/Indexer.h"

namespace clice::testing {

TEST(Indexer, Basic) {
    config::IndexOptions options;
    options.dir = path::join(".", "temp");
    auto error = fs::create_directories(options.dir);

    CompilationDatabase database;
    auto prefix = path::join(test_dir(), "indexer");
    auto foo = path::real_path(path::join(prefix, "foo.cpp"));
    auto main = path::real_path(path::join(prefix, "main.cpp"));
    database.updateCommand(foo, std::format("clang++ {}", foo));
    database.updateCommand(main, std::format("clang++ {}", main));

    Indexer indexer(options, database);
    indexer.loadFromDisk();

    auto p1 = indexer.index(main);
    auto p2 = indexer.index(foo);
    async::run(p1, p2);

    auto kind =
        RelationKind(RelationKind::Reference, RelationKind::Definition, RelationKind::Declaration);
    proto::DeclarationParams params{
        .textDocument = {.uri = SourceConverter::toURI(foo)},
        .position = {2, 5}
    };
    auto lookup = indexer.lookup(params, kind);
    auto&& [result] = async::run(lookup);

    indexer.saveToDisk();

    Indexer indexer2(options, database);
    indexer2.loadFromDisk();

    auto lookup2 = indexer2.lookup(params, kind);
    auto&& [result2] = async::run(lookup2);

    print("Result: {}\n", json::serialize(result));

    EXPECT_EQ(result, result2);
}

TEST(Indexer, Assert) {
    config::IndexOptions options;
    options.dir = path::join(".", "temp");
    auto error = fs::create_directories(options.dir);

    CompilationDatabase database;
    auto prefix = path::join(test_dir(), "indexer");
    auto main = path::real_path(path::join(prefix, "main.cpp"));
    database.updateCommand(main, std::format("clang++ {}", main));

    Indexer indexer(options, database);
    indexer.loadFromDisk();

    auto p1 = indexer.index(main);
    async::run(p1);

    indexer.saveToDisk();
}

TEST(Indexer, IndexAll) {
    config::IndexOptions options;
    options.dir = path::join(".", "temp");
    auto error = fs::create_directories(options.dir);

    CompilationDatabase database;
    database.updateCommands("/home/ykiko/C++/clice/build/compile_commands.json");

    Indexer indexer(options, database);
    indexer.loadFromDisk();

    /// auto p1 = indexer.indexAll();
    auto p1 = indexer.indexAll();
    async::run(p1);

    auto p2 = indexer.mergeAll();
    async::run(p2);

    indexer.saveToDisk();
}

TEST(Indexer, Debug) {
    config::IndexOptions options;
    options.dir = path::join(".", "temp");
    auto error = fs::create_directories(options.dir);

    CompilationDatabase database;
    database.updateCommands("/home/ykiko/C++/clice/build/compile_commands.json");

    Indexer indexer(options, database);
    indexer.loadFromDisk();

    /// auto p1 =
    /// indexer.index("/home/ykiko/C++/clice/build/_deps/libuv-src/src/unix/random-sysctl-linux.c");
    // auto p = indexer.merge("/usr/include/assert.h");
    // async::run(p);

    /// indexer.dumpForTest("/usr/include/assert.h");

    indexer.saveToDisk();
}

}  // namespace clice::testing
