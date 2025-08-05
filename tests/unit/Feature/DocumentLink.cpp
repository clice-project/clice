#include "Test/Tester.h"
#include "Feature/DocumentLink.h"

namespace clice::testing {

namespace {

struct DocumentLink : TestFixture {
    index::Shared<feature::DocumentLinks> result;

    using Self = DocumentLink;

    void run() {
        Tester::compile();
        result = feature::indexDocumentLink(*unit);
    }

    void EXPECT_LINK(this Self& self,
                     uint32_t index,
                     llvm::StringRef name,
                     llvm::StringRef path,
                     LocationChain chain = LocationChain()) {
        auto& link = self.result[self.unit->interested_file()][index];

        EXPECT_EQ(link.range, self.range(name, "main.cpp"), chain);

        llvm::SmallString<64> file = {link.file};
        path::remove_dots(file);

        EXPECT_EQ(file, path, chain);
    }

    void dump() {
        clice::println("{}", clice::dump(result[unit->interested_file()]));
    }
};

TEST_F(DocumentLink, Include) {
    add_files("main.cpp", R"cpp(
#[test.h]

#[pragma_once.h]
#pragma once

#[guard_macro.h]
#ifndef TEST3_H
#define TEST3_H
#endif

#[main.cpp]
#include @0["test.h"$]
#include @1["test.h"$]
#include @2["pragma_once.h"$]
#include @3["pragma_once.h"$]
#include @4["guard_macro.h"$]
#include @5["guard_macro.h"$]
)cpp");

    run();

    auto& links = result[unit->interested_file()];
    EXPECT_EQ(links.size(), 6);
    EXPECT_LINK(0, "0", "test.h");
    EXPECT_LINK(1, "1", "test.h");
    EXPECT_LINK(2, "2", "pragma_once.h");
    EXPECT_LINK(3, "3", "pragma_once.h");
    EXPECT_LINK(4, "4", "guard_macro.h");
    EXPECT_LINK(5, "5", "guard_macro.h");
}

TEST_F(DocumentLink, HasInclude) {
    add_files("main.cpp", R"cpp(
#[test.h]

#[main.cpp]
#include @0["test.h"]

#if __has_include(@1["test.h"])
#endif

#if __has_include("test2.h")
#endif
)cpp");

    run();

    auto& links = result[unit->interested_file()];
    EXPECT_EQ(links.size(), 2);
    EXPECT_LINK(0, "0", "test.h");
    EXPECT_LINK(1, "1", "test.h");
}

}  // namespace

}  // namespace clice::testing
