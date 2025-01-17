#include "Test/CTest.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

TEST(Directive, Include) {
    const char* test = "";

    const char* test2 = R"cpp(
#include "test.h"
)cpp";

    const char* main = R"cpp(
#$(0)include "test.h"
#$(1)include "test2.h"
#$(2)include "test3.h"
)cpp";

    Tester tester("main.cpp", main);
    tester.addFile("./test.h", test);
    tester.addFile("./test2.h", test2);
    tester.addFile("./test3.h", "");
    tester.run();

    auto& info = tester.info;
    auto& includes = info->directives()[info->srcMgr().getMainFileID()].includes;

    auto EXPECT_INCLUDE = [&](std::size_t index,
                              llvm::StringRef position,
                              llvm::StringRef path,
                              std::source_location current = std::source_location::current()) {
        auto& include = includes[index];
        EXPECT_EQ(SourceConverter().toPosition(include.loc, info->srcMgr()), tester.pos(position));
        EXPECT_EQ(include.path, path);
    };

    EXPECT_EQ(includes.size(), 3);
    EXPECT_INCLUDE(0, "0", "test.h");
    EXPECT_INCLUDE(1, "1", "test2.h");
    EXPECT_INCLUDE(2, "2", "test3.h");
}

TEST(Directive, Condition) {
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

    Tester tester("main.cpp", code);
    tester.run("-std=c++23");
    auto& info = tester.info;
    auto& conditions = info->directives()[info->srcMgr().getMainFileID()].conditions;

    auto EPXECT_CON = [&](std::size_t index,
                          Condition::BranchKind kind,
                          llvm::StringRef position,
                          std::source_location current = std::source_location::current()) {
        auto& condition = conditions[index];
        EXPECT_EQ(condition.kind, kind, current);
        EXPECT_EQ(SourceConverter().toPosition(condition.loc, info->srcMgr()),
                  tester.pos(position),
                  current);
    };

    EXPECT_EQ(conditions.size(), 8);
    EPXECT_CON(0, Condition::BranchKind::If, "0");
    EPXECT_CON(1, Condition::BranchKind::Elif, "1");
    EPXECT_CON(2, Condition::BranchKind::Else, "2");
    EPXECT_CON(3, Condition::BranchKind::EndIf, "3");
    EPXECT_CON(4, Condition::BranchKind::Ifdef, "4");
    EPXECT_CON(5, Condition::BranchKind::Elifdef, "5");
    EPXECT_CON(6, Condition::BranchKind::Else, "6");
    EPXECT_CON(7, Condition::BranchKind::EndIf, "7");
}

TEST(Directive, Macro) {
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

    Tester tester("main.cpp", code);
    tester.run();
    auto& info = tester.info;
    auto& macros = info->directives()[info->srcMgr().getMainFileID()].macros;

    auto EXPECT_MACRO = [&](std::size_t index,
                            MacroRef::Kind kind,
                            llvm::StringRef position,
                            std::source_location current = std::source_location::current()) {
        auto& macro = macros[index];
        EXPECT_EQ(macro.kind, kind, current);
        EXPECT_EQ(SourceConverter().toPosition(macro.loc, info->srcMgr()),
                  tester.pos(position),
                  current);
    };

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
