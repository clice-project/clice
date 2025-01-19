#include "Test/Test.h"
#include "Server/Indexer.h"

namespace clice::testing {

TEST(Indexer, Basic) {
    CompilationDatabase database;
    Indexer indexer(database);
}

}  // namespace clice::testing
