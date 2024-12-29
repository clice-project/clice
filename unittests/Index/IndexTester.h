#include "../Test.h"
#include "Index/SymbolIndex.h"

namespace clice {

struct IndexTester : Tester {
    std::vector<char> binary;

    using Tester::Tester;

    IndexTester& run(const char* standard = "-std=c++20") {
        Tester::run(standard);
        index::test(info);
        return *this;
    }

    IndexTester& GotoDefinition(llvm::StringRef cursor,
                                llvm::StringRef target,
                                std::source_location current = std::source_location::current()) {
        return *this;
    }
};

}  // namespace clice
