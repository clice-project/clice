#include "Test/Tester.h"

namespace clice::testing {

namespace {

struct Directive : ::testing::Test, Tester {
    llvm::ArrayRef<Include> includes;
    llvm::ArrayRef<HasInclude> has_includes;
    llvm::ArrayRef<Condition> conditions;
    llvm::ArrayRef<MacroRef> macros;
    llvm::ArrayRef<Pragma> pragmas;

    using Self = Directive;

    void run(const char* standard = "-std=c++20") {
        Tester::compile("-std=c++23");
        auto fid = unit->interested_file();
        includes = unit->directives()[fid].includes;
        has_includes = unit->directives()[fid].has_includes;
        conditions = unit->directives()[fid].conditions;
        macros = unit->directives()[fid].macros;
        pragmas = unit->directives()[fid].pragmas;
    }

    void EXPECT_INCLUDE(this Self& self,
                        std::size_t index,
                        llvm::StringRef position,
                        llvm::StringRef path,
                        LocationChain chain = LocationChain()) {
        auto& include = self.includes[index];
        auto [_, offset] = self.unit->decompose_location(include.location);
        EXPECT_EQ(offset, self["main.cpp", position], chain);

        /// FIXME: Implicit relative path ...
        llvm::SmallString<64> target = include.skipped ? "" : self.unit->file_path(include.fid);
        path::remove_dots(target);

        EXPECT_EQ(target, path, chain);
    }

    void EXPECT_HAS_INCLUDE(this Self& self,
                            std::size_t index,
                            llvm::StringRef position,
                            llvm::StringRef path,
                            LocationChain chain = LocationChain()) {
        auto& hasInclude = self.has_includes[index];
        auto [_, offset] = self.unit->decompose_location(hasInclude.location);
        EXPECT_EQ(offset, self["main.cpp", position], chain);

        /// FIXME:
        llvm::SmallString<64> target =
            hasInclude.fid.isValid() ? self.unit->file_path(hasInclude.fid) : "";
        path::remove_dots(target);

        EXPECT_EQ(target, path, chain);
    }

    void EXPECT_CON(this Self& self,
                    std::size_t index,
                    Condition::BranchKind kind,
                    llvm::StringRef position,
                    LocationChain chain = LocationChain()) {
        auto& condition = self.conditions[index];
        auto [_, offset] = self.unit->decompose_location(condition.loc);
        EXPECT_EQ(condition.kind, kind, chain);
        EXPECT_EQ(offset, self["main.cpp", position], chain);
    }

    void EXPECT_MACRO(this Self& self,
                      std::size_t index,
                      MacroRef::Kind kind,
                      llvm::StringRef position,
                      LocationChain chain = LocationChain()) {
        auto& macro = self.macros[index];
        auto [_, offset] = self.unit->decompose_location(macro.loc);
        EXPECT_EQ(macro.kind, kind, chain);
        EXPECT_EQ(offset, self["main.cpp", position], chain);
    }

    void EXPECT_PRAGMA(this Self& self,
                       std::size_t index,
                       Pragma::Kind kind,
                       llvm::StringRef position,
                       llvm::StringRef text,
                       LocationChain chain = LocationChain()) {
        auto& pragma = self.pragmas[index];
        auto [_, offset] = self.unit->decompose_location(pragma.loc);
        EXPECT_EQ(pragma.kind, kind, chain);
        EXPECT_EQ(pragma.stmt, text, chain);
        EXPECT_EQ(offset, self["main.cpp", position], chain);
    }
};

TEST_F(Directive, Include) {
    add_files("main.cpp", R"cpp(
#[test.h]

#[pragma_once.h]
#pragma once

#[guard_macro.h]
#ifndef TEST3_H
#define TEST3_H
#endif

#[main.cpp]
#$(0)include "test.h"
#$(1)include "test.h"
#$(2)include "pragma_once.h"
#$(3)include "pragma_once.h"
#$(4)include "guard_macro.h"
#$(5)include "guard_macro.h"
)cpp");

    run();

    EXPECT_EQ(includes.size(), 6);
    EXPECT_INCLUDE(0, "0", "test.h");
    EXPECT_INCLUDE(1, "1", "test.h");
    EXPECT_INCLUDE(2, "2", "pragma_once.h");
    EXPECT_INCLUDE(3, "3", "");
    EXPECT_INCLUDE(4, "4", "guard_macro.h");
    EXPECT_INCLUDE(5, "5", "");

    /// TODO: test include source range.
}

TEST_F(Directive, HasInclude) {
    add_files("main.cpp", R"cpp(
#[test.h]

#[main.cpp]
#include "test.h"
#if __has_include($(0)"test.h")
#endif

#if __has_include($(1)"test2.h")
#endif
)cpp");
    run();

    EXPECT_EQ(has_includes.size(), 2);
    EXPECT_HAS_INCLUDE(0, "0", "test.h");
    EXPECT_HAS_INCLUDE(1, "1", "");
}

TEST_F(Directive, Condition) {
    add_main("main.cpp", R"cpp(
#$(0)if 0
#$(1)elif 1
#$(2)else
#$(3)endif

#$(4)ifdef name
#$(5)elifdef name
#$(6)else
#$(7)endif
)cpp");
    run("-std=c++23");

    EXPECT_EQ(conditions.size(), 8);
    EXPECT_CON(0, Condition::BranchKind::If, "0");
    EXPECT_CON(1, Condition::BranchKind::Elif, "1");
    EXPECT_CON(2, Condition::BranchKind::Else, "2");
    EXPECT_CON(3, Condition::BranchKind::EndIf, "3");
    EXPECT_CON(4, Condition::BranchKind::Ifdef, "4");
    EXPECT_CON(5, Condition::BranchKind::Elifdef, "5");
    EXPECT_CON(6, Condition::BranchKind::Else, "6");
    EXPECT_CON(7, Condition::BranchKind::EndIf, "7");
}

TEST_F(Directive, Macro) {
    const char* code = R"cpp(
#define $(0)expr(v) v

#ifdef $(1)expr
int x = $(2)expr(1);
#endif

#undef $(3)expr

#define $(4)expr(v) v

#ifdef $(5)expr
int y = $(6)expr($(7)expr(1));
#endif

#undef $(8)expr

)cpp";

    add_main("main.cpp", code);
    run();

    EXPECT_EQ(macros.size(), 9);
    EXPECT_MACRO(0, MacroRef::Kind::Def, "0");
    EXPECT_MACRO(1, MacroRef::Kind::Ref, "1");
    EXPECT_MACRO(2, MacroRef::Kind::Ref, "2");
    EXPECT_MACRO(3, MacroRef::Kind::Undef, "3");
    EXPECT_MACRO(4, MacroRef::Kind::Def, "4");
    EXPECT_MACRO(5, MacroRef::Kind::Ref, "5");
    EXPECT_MACRO(6, MacroRef::Kind::Ref, "6");
    EXPECT_MACRO(7, MacroRef::Kind::Ref, "7");
    EXPECT_MACRO(8, MacroRef::Kind::Undef, "8");
}

TEST_F(Directive, Pragma) {
    const char* code = R"cpp(
$(0)#pragma GCC poison printf sprintf fprintf
$(1)#pragma region
$(2)#pragma endregion
)cpp";

    add_main("main.cpp", code);
    run();

    EXPECT_EQ(3, pragmas.size());
    EXPECT_PRAGMA(0, Pragma::Kind::Other, "0", "#pragma GCC poison printf sprintf fprintf");
    EXPECT_PRAGMA(1, Pragma::Kind::Region, "1", "#pragma region");
    EXPECT_PRAGMA(2, Pragma::Kind::EndRegion, "2", "#pragma endregion");
}

}  // namespace

}  // namespace clice::testing
