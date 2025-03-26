#include "Test/CTest.h"
#include "Server/SourceConverter.h"

namespace clice::testing {

namespace {

struct Directive : ::testing::Test, Tester {
    clang::SourceManager* SM;
    llvm::ArrayRef<Include> includes;
    llvm::ArrayRef<HasInclude> hasIncludes;
    llvm::ArrayRef<Condition> conditions;
    llvm::ArrayRef<MacroRef> macros;
    llvm::ArrayRef<Pragma> pragmas;

    void run(const char* standard = "-std=c++20") {
        Tester::compile("-std=c++23");
        SM = &info->srcMgr();
        auto fid = SM->getMainFileID();
        includes = info->directives()[fid].includes;
        hasIncludes = info->directives()[fid].hasIncludes;
        conditions = info->directives()[fid].conditions;
        macros = info->directives()[fid].macros;
        pragmas = info->directives()[fid].pragmas;
    }

    void EXPECT_INCLUDE(std::size_t index,
                        llvm::StringRef position,
                        llvm::StringRef path,
                        LocationChain chain = LocationChain()) {
        auto& include = includes[index];
        auto [_, offset] = info->getDecomposedLoc(include.location);
        EXPECT_EQ(offset, this->offset(position), chain);
        EXPECT_EQ(include.skipped ? "" : info->getFilePath(include.fid), path, chain);
    }

    void EXPECT_HAS_INCLUDE(std::size_t index,
                            llvm::StringRef position,
                            llvm::StringRef path,
                            LocationChain chain = LocationChain()) {
        auto& hasInclude = hasIncludes[index];
        auto [_, offset] = info->getDecomposedLoc(hasInclude.location);
        EXPECT_EQ(offset, this->offset(position), chain);
        EXPECT_EQ(hasInclude.fid.isValid() ? info->getFilePath(hasInclude.fid) : "", path, chain);
    }

    void EXPECT_CON(std::size_t index,
                    Condition::BranchKind kind,
                    llvm::StringRef position,
                    LocationChain chain = LocationChain()) {
        auto& condition = conditions[index];
        auto [_, offset] = info->getDecomposedLoc(condition.loc);
        EXPECT_EQ(condition.kind, kind, chain);
        EXPECT_EQ(offset, this->offset(position), chain);
    }

    void EXPECT_MACRO(std::size_t index,
                      MacroRef::Kind kind,
                      llvm::StringRef position,
                      LocationChain chain = LocationChain()) {
        auto& macro = macros[index];
        auto [_, offset] = info->getDecomposedLoc(macro.loc);
        EXPECT_EQ(macro.kind, kind, chain);
        EXPECT_EQ(offset, this->offset(position), chain);
    }

    void EXPECT_PRAGMA(std::size_t index,
                       Pragma::Kind kind,
                       llvm::StringRef position,
                       llvm::StringRef text,
                       LocationChain chain = LocationChain()) {
        auto& pragma = pragmas[index];
        auto [_, offset] = info->getDecomposedLoc(pragma.loc);
        EXPECT_EQ(pragma.kind, kind, chain);
        EXPECT_EQ(pragma.stmt, text, chain);
        EXPECT_EQ(offset, this->offset(position), chain);
    }
};

TEST_F(Directive, Include) {
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
#$(0)include "test.h"
#$(1)include "test.h"
#$(2)include "pragma_once.h"
#$(3)include "pragma_once.h"
#$(4)include "guard_macro.h"
#$(5)include "guard_macro.h"
)cpp";

    addMain("main.cpp", main);

    auto ptest = path::join(".", "test.h");
    auto ppragma_once = path::join(".", "pragma_once.h");
    auto pguard_macro = path::join(".", "guard_macro.h");

    addFile(ptest, test);
    addFile(ppragma_once, pragma_once);
    addFile(pguard_macro, guard_macro);
    run();

    EXPECT_EQ(includes.size(), 6);
    EXPECT_INCLUDE(0, "0", ptest);
    EXPECT_INCLUDE(1, "1", ptest);
    EXPECT_INCLUDE(2, "2", ppragma_once);
    EXPECT_INCLUDE(3, "3", "");
    EXPECT_INCLUDE(4, "4", pguard_macro);
    EXPECT_INCLUDE(5, "5", "");

    /// TODO: test include source range.
}

TEST_F(Directive, HasInclude) {
    const char* test = "";
    const char* main = R"cpp(
#include "test.h"
#if __has_include($(0)"test.h")
#endif

#if __has_include($(1)"test2.h")
#endif
)cpp";

    addMain("main.cpp", main);

    auto path = path::join(".", "test.h");
    addFile(path, test);

    run();

    EXPECT_EQ(hasIncludes.size(), 2);
    EXPECT_HAS_INCLUDE(0, "0", path);
    EXPECT_HAS_INCLUDE(1, "1", "");
}

TEST_F(Directive, Condition) {
    const char* code = R"cpp(
#$(0)if 0

#$(1)elif 1

#$(2)else

#$(3)endif

#$(4)ifdef name

#$(5)elifdef name

#$(6)else

#$(7)endif
)cpp";

    addMain("main.cpp", code);
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

    addMain("main.cpp", code);
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

    addMain("main.cpp", code);
    run();

    EXPECT_EQ(3, pragmas.size());
    EXPECT_PRAGMA(0, Pragma::Kind::Other, "0", "#pragma GCC poison printf sprintf fprintf");
    EXPECT_PRAGMA(1, Pragma::Kind::Region, "1", "#pragma region");
    EXPECT_PRAGMA(2, Pragma::Kind::EndRegion, "2", "#pragma endregion");
}

}  // namespace

}  // namespace clice::testing
