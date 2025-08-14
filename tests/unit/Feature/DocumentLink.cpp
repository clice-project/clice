#include "Test/Tester.h"
#include "Feature/DocumentLink.h"

namespace clice::testing {

namespace {

suite<"DocumentLink"> document_link = [] {
    Tester tester;
    std::vector<feature::DocumentLink> links;

    auto run = [&](llvm::StringRef source) {
        tester.clear();
        tester.add_files("main.cpp", source);
        tester.compile();
        links = feature::document_links(*tester.unit);
    };

    auto expect_link = [&](std::size_t index, llvm::StringRef name, llvm::StringRef path) {
        auto& link = links[index];
        auto range = tester.range(name, "main.cpp");
        expect(that % link.range.begin == range.begin);
        expect(that % link.range.end == range.end);

        /// FIXME: workaround to make `./file` to `file`
        llvm::SmallString<64> file = {link.file};
        path::remove_dots(file);
        expect(that % file == path);
    };

    test("Include") = [&] {
        run(R"cpp(
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

        expect(that % links.size() == 6);
        expect_link(0, "0", "test.h");
        expect_link(1, "1", "test.h");
        expect_link(2, "2", "pragma_once.h");
        expect_link(3, "3", "pragma_once.h");
        expect_link(4, "4", "guard_macro.h");
        expect_link(5, "5", "guard_macro.h");
    };

    test("HasInclude") = [&] {
        run(R"cpp(
#[test.h]

#[main.cpp]
#include @0["test.h"]

#if __has_include(@1["test.h"])
#endif

#if __has_include("test2.h")
#endif
)cpp");

        expect(that % links.size() == 2);
        expect_link(0, "0", "test.h");
        expect_link(1, "1", "test.h");
    };
};

}  // namespace

}  // namespace clice::testing
