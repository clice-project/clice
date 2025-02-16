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

    Indexer indexer(database, options);
    indexer.load();

    indexer.add(main);
    indexer.add(foo);
    async::run();

    auto kind =
        RelationKind(RelationKind::Reference, RelationKind::Definition, RelationKind::Declaration);
    proto::ReferenceParams params;
    params.textDocument = {.uri = SourceConverter::toURI(foo)};
    params.position = {2, 5};

    auto lookup = indexer.lookup(params, kind);
    auto&& [result] = async::run(lookup);

    indexer.save();

    Indexer indexer2(database, options);
    indexer2.load();

    auto lookup2 = indexer2.lookup(params, kind);
    auto&& [result2] = async::run(lookup2);

    /// FIXME: Adjust order?
    /// EXPECT_EQ(result, result2);
}

}  // namespace clice::testing
