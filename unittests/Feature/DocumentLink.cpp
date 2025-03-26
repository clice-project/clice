#include "Test/CTest.h"
#include "Feature/DocumentLink.h"

namespace clice::testing {

namespace {

struct DocumentLink : Test {
    index::Shared<feature::DocumentLinkResult> result;

    void run(llvm::StringRef code) {
        addMain("main.cpp", code);
        Tester::run();
        result = feature::indexDocumentLink(*info);
    }

    void EXPECT_LINK(uint32_t index,
                     llvm::StringRef begin,
                     llvm::StringRef end,
                     llvm::StringRef path,
                     LocationChain chain = LocationChain()) {
        auto& link = result[info->getInterestedFile()][index];
        EXPECT_EQ(link.range.begin, offset(begin), chain);
        EXPECT_EQ(link.range.end, offset(end), chain);
        EXPECT_EQ(link.file, path, chain);
    }

    void dump() {
        println("{}", clice::dump(result[info->getInterestedFile()]));
    }
};

TEST_F(DocumentLink, Include) {
    const char* test = "";

    const char* test2 = R"cpp(
#include "test.h"
)cpp";

    const char* pragma_once = R"cpp(
#pragma once
)cpp";

    const char* guard_macro = R"cpp(
#ifndef TEST3_H
#define TEST3_H
#endif
)cpp";

    const char* main = R"cpp(
#include $(0)"test.h"$(0e)
#include $(1)"test.h"$(1e)
#include $(2)"pragma_once.h"$(2e)
#include $(3)"pragma_once.h"$(3e)
#include $(4)"guard_macro.h"$(4e)
#include $(5)"guard_macro.h"$(5e)
)cpp";

    auto ptest = path::join(".", "test.h");
    auto ppragma_once = path::join(".", "pragma_once.h");
    auto pguard_macro = path::join(".", "guard_macro.h");

    addFile(ptest, test);
    addFile(ppragma_once, pragma_once);
    addFile(pguard_macro, guard_macro);
    run(main);

    auto& links = result[info->getInterestedFile()];
    EXPECT_EQ(links.size(), 6);
    EXPECT_LINK(0, "0", "0e", ptest);
    EXPECT_LINK(1, "1", "1e", ptest);
    EXPECT_LINK(2, "2", "2e", ppragma_once);
    EXPECT_LINK(3, "3", "3e", ppragma_once);
    EXPECT_LINK(4, "4", "4e", pguard_macro);
    EXPECT_LINK(5, "5", "5e", pguard_macro);
}

TEST_F(DocumentLink, HasInclude) {
    const char* test = "";
    const char* main = R"cpp(
#include $(0)"test.h"$(0e)
#if __has_include($(1)"test.h"$(1e))
#endif

#if __has_include("test2.h")
#endif
)cpp";

    auto path = path::join(".", "test.h");
    addFile(path, test);

    run(main);

    auto& links = result[info->getInterestedFile()];
    EXPECT_EQ(links.size(), 2);
    EXPECT_LINK(0, "0", "0e", path);
    EXPECT_LINK(1, "1", "1e", path);
}

}  // namespace

}  // namespace clice::testing
