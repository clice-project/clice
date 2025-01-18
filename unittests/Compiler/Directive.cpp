#include "Test/CTest.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

struct Directive : ::testing::Test, Tester {
    clang::SourceManager* SM;
    llvm::ArrayRef<Include> includes;
    llvm::ArrayRef<HasInclude> hasIncludes;
    llvm::ArrayRef<Condition> conditions;
    llvm::ArrayRef<MacroRef> macros;

    void run(const char* standard = "-std=c++20") {
        Tester::run("-std=c++23");
        SM = &info->srcMgr();
        auto fid = SM->getMainFileID();
        includes = info->directives()[fid].includes;
        hasIncludes = info->directives()[fid].hasIncludes;
        conditions = info->directives()[fid].conditions;
        macros = info->directives()[fid].macros;
    }

    void EXPECT_INCLUDE(std::size_t index,
                        llvm::StringRef position,
                        llvm::StringRef path,
                        std::source_location current = std::source_location::current()) {
        auto& include = includes[index];
        auto entry = SM->getFileEntryRefForID(include.fid);
        EXPECT_EQ(SourceConverter().toPosition(include.location, *SM), pos(position), current);
        EXPECT_EQ(entry ? entry->getName() : "", path, current);
    }

    void EXPECT_HAS_INCLUDE(std::size_t index,
                            llvm::StringRef position,
                            llvm::StringRef path,
                            std::source_location current = std::source_location::current()) {
        auto& hasInclude = hasIncludes[index];
        EXPECT_EQ(SourceConverter().toPosition(hasInclude.location, *SM), pos(position), current);
        EXPECT_EQ(hasInclude.path, path, current);
    }

    void EXPECT_CON(std::size_t index,
                    Condition::BranchKind kind,
                    llvm::StringRef position,
                    std::source_location current = std::source_location::current()) {
        auto& condition = conditions[index];
        EXPECT_EQ(condition.kind, kind, current);
        EXPECT_EQ(SourceConverter().toPosition(condition.loc, *SM), pos(position), current);
    }

    void EXPECT_MACRO(std::size_t index,
                      MacroRef::Kind kind,
                      llvm::StringRef position,
                      std::source_location current = std::source_location::current()) {
        auto& macro = macros[index];
        EXPECT_EQ(macro.kind, kind, current);
        EXPECT_EQ(SourceConverter().toPosition(macro.loc, *SM), pos(position), current);
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

    using Path = llvm::SmallString<128>;
    Path ptest, ppragma_once, pguard_macro;
    path::append(ptest, ".", "test.h");
    path::append(ppragma_once, ".", "pragma_once.h");
    path::append(pguard_macro, ".", "guard_macro.h");

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
}

TEST_F(Directive, HasInclude) {
    const char* test = "";

    const char* main = R"cpp(
#if __has_include($(0)"test.h")
#endif
)cpp";

    addMain("main.cpp", main);

    llvm::SmallString<128> path;
    path::append(path, ".", "test.h");
    addFile(path, test);
    
    run();

    EXPECT_EQ(hasIncludes.size(), 1);
    EXPECT_HAS_INCLUDE(0, "0", path);
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

}  // namespace

}  // namespace clice::testing
