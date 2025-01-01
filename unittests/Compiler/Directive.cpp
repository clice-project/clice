#include "../Test.h"
#include "Compiler/Compiler.h"

namespace clice {

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
    auto& includes = info.directive(info.srcMgr().getMainFileID()).includes;

    tester.equal(includes.size(), 3)
        .expect("0", includes[0].include)
        .equal("./test.h", includes[0].path)
        .expect("1", includes[1].include)
        .equal("./test2.h", includes[1].path)
        .expect("2", includes[2].include)
        .equal("./test3.h", includes[2].path);
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
    auto& conditions = info.directive(info.srcMgr().getMainFileID()).conditions;

    tester.equal(conditions.size(), 8)
        .equal(conditions[0].kind, Condition::BranchKind::If)
        .expect("0", conditions[0].loc)
        .equal(conditions[1].kind, Condition::BranchKind::Elif)
        .expect("1", conditions[1].loc)
        .equal(conditions[2].kind, Condition::BranchKind::Else)
        .expect("2", conditions[2].loc)
        .equal(conditions[3].kind, Condition::BranchKind::EndIf)
        .expect("3", conditions[3].loc)
        .equal(conditions[4].kind, Condition::BranchKind::Ifdef)
        .expect("4", conditions[4].loc)
        .equal(conditions[5].kind, Condition::BranchKind::Elifdef)
        .expect("5", conditions[5].loc)
        .equal(conditions[6].kind, Condition::BranchKind::Else)
        .expect("6", conditions[6].loc)
        .equal(conditions[7].kind, Condition::BranchKind::EndIf)
        .expect("7", conditions[7].loc);
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
    auto& macros = info.directive(info.srcMgr().getMainFileID()).macros;

    tester.equal(macros.size(), 9)
        .equal(macros[0].kind, MacroRef::Kind::Def)
        .expect("0", macros[0].loc)
        .equal(macros[1].kind, MacroRef::Kind::Ref)
        .expect("1", macros[1].loc)
        .equal(macros[2].kind, MacroRef::Kind::Ref)
        .expect("2", macros[2].loc)
        .equal(macros[3].kind, MacroRef::Kind::Undef)
        .expect("3", macros[3].loc)
        .equal(macros[4].kind, MacroRef::Kind::Def)
        .expect("4", macros[4].loc)
        .equal(macros[5].kind, MacroRef::Kind::Ref)
        .expect("5", macros[5].loc)
        .equal(macros[6].kind, MacroRef::Kind::Ref)
        .expect("6", macros[6].loc)
        .equal(macros[7].kind, MacroRef::Kind::Ref)
        .expect("7", macros[7].loc)
        .equal(macros[8].kind, MacroRef::Kind::Undef)
        .expect("8", macros[8].loc);
}

}  // namespace

}  // namespace clice
