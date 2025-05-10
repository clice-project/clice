#include <iostream>

#include "Test/Test.h"
#include "Support/GlobPattern.h"

namespace clice::testing {

#define PATDEF(NAME, PAT)                                                                          \
    const char* PatString_##NAME = PAT;                                                            \
    auto Res##NAME = clice::GlobPattern::create(PatString_##NAME, 100);                            \
    if(!Res##NAME.has_value()) {                                                                   \
        std::cout << Res##NAME.error() << '\n';                                                    \
    }                                                                                              \
    assert(Res##NAME.has_value());                                                                 \
    auto NAME = Res##NAME.value();

TEST(GlobPattern, Simple) {
    PATDEF(Pat1, "node_modules")
    EXPECT_EQ(Pat1.match("node_modules"), true);
    EXPECT_EQ(Pat1.match("node_module"), false);
    EXPECT_EQ(Pat1.match("/node_modules"), false);
    EXPECT_EQ(Pat1.match("test/node_modules"), false);

    PATDEF(Pat2, "test.txt")
    EXPECT_EQ(Pat2.match("test.txt"), true);
    EXPECT_EQ(Pat2.match("test?txt"), false);
    EXPECT_EQ(Pat2.match("/text.txt"), false);
    EXPECT_EQ(Pat2.match("test/test.txt"), false);

    PATDEF(Pat3, "test(.txt")
    EXPECT_EQ(Pat3.match("test(.txt"), true);
    EXPECT_EQ(Pat3.match("test?txt"), false);

    PATDEF(Pat4, "qunit")
    EXPECT_EQ(Pat4.match("qunit"), true);
    EXPECT_EQ(Pat4.match("qunit.css"), false);
    EXPECT_EQ(Pat4.match("test/qunit"), false);

    PATDEF(Pat5, "/DNXConsoleApp/**/*.cs")
    EXPECT_EQ(Pat5.match("/DNXConsoleApp/Program.cs"), true);
    EXPECT_EQ(Pat5.match("/DNXConsoleApp/foo/Program.cs"), true);
}

TEST(GlobPattern, DotHidden) {
    PATDEF(Pat1, ".*");
    EXPECT_EQ(Pat1.match(".git"), true);
    EXPECT_EQ(Pat1.match(".hidden.txt"), true);
    EXPECT_EQ(Pat1.match("git"), false);
    EXPECT_EQ(Pat1.match("hidden.txt"), false);
    EXPECT_EQ(Pat1.match("path/.git"), false);
    EXPECT_EQ(Pat1.match("path/.hidden.txt"), false);

    PATDEF(Pat2, "**/.*");
    EXPECT_EQ(Pat2.match(".git"), true);
    EXPECT_EQ(Pat2.match("/.git"), true);
    EXPECT_EQ(Pat2.match(".hidden.txt"), true);
    EXPECT_EQ(Pat2.match("git"), false);
    EXPECT_EQ(Pat2.match("hidden.txt"), false);
    EXPECT_EQ(Pat2.match("path/.git"), true);
    EXPECT_EQ(Pat2.match("path/.hidden.txt"), true);
    EXPECT_EQ(Pat2.match("/path/.git"), true);
    EXPECT_EQ(Pat2.match("/path/.hidden.txt"), true);
    EXPECT_EQ(Pat2.match("path/git"), false);
    EXPECT_EQ(Pat2.match("pat.h/hidden.txt"), false);

    PATDEF(Pat3, "._*");
    EXPECT_EQ(Pat3.match("._git"), true);
    EXPECT_EQ(Pat3.match("._hidden.txt"), true);
    EXPECT_EQ(Pat3.match("git"), false);
    EXPECT_EQ(Pat3.match("hidden.txt"), false);
    EXPECT_EQ(Pat3.match("path/._git"), false);
    EXPECT_EQ(Pat3.match("path/._hidden.txt"), false);

    PATDEF(Pat4, "**/._*");
    EXPECT_EQ(Pat4.match("._git"), true);
    EXPECT_EQ(Pat4.match("._hidden.txt"), true);
    EXPECT_EQ(Pat4.match("git"), false);
    EXPECT_EQ(Pat4.match("hidden._txt"), false);
    EXPECT_EQ(Pat4.match("path/._git"), true);
    EXPECT_EQ(Pat4.match("path/._hidden.txt"), true);
    EXPECT_EQ(Pat4.match("/path/._git"), true);
    EXPECT_EQ(Pat4.match("/path/._hidden.txt"), true);
    EXPECT_EQ(Pat4.match("path/git"), false);
    EXPECT_EQ(Pat4.match("pat.h/hidden._txt"), false);
}

TEST(GlobPattern, EscapeCharacter) {
    PATDEF(Pat1, R"(\*star)")
    EXPECT_EQ(Pat1.match("*star"), true);

    PATDEF(Pat2, R"(\{\*\})")
    EXPECT_EQ(Pat2.match("{*}"), true);
}

TEST(GlobPattern, BracketExpr) {
    PATDEF(Pat1, R"([a-zA-Z\]])")
    EXPECT_EQ(Pat1.match(R"(])"), true);
    EXPECT_EQ(Pat1.match(R"([)"), false);
    EXPECT_EQ(Pat1.match(R"(s)"), true);
    EXPECT_EQ(Pat1.match(R"(S)"), true);
    EXPECT_EQ(Pat1.match(R"(0)"), false);

    PATDEF(Pat2, R"([\^a-zA-Z""\\])")
    EXPECT_EQ(Pat2.match(R"(")"), true);
    EXPECT_EQ(Pat2.match(R"(^)"), true);
    EXPECT_EQ(Pat2.match(R"(\)"), true);
    EXPECT_EQ(Pat2.match(R"(")"), true);
    EXPECT_EQ(Pat2.match(R"(x)"), true);
    EXPECT_EQ(Pat2.match(R"(X)"), true);
    EXPECT_EQ(Pat2.match(R"(0)"), false);

    PATDEF(Pat3, R"([!0-9a-fA-F\-+\*])")
    EXPECT_EQ(Pat3.match("1"), false);
    EXPECT_EQ(Pat3.match("*"), false);
    EXPECT_EQ(Pat3.match("s"), true);
    EXPECT_EQ(Pat3.match("S"), true);
    EXPECT_EQ(Pat3.match("H"), true);
    EXPECT_EQ(Pat3.match("]"), true);

    PATDEF(Pat4, R"([^\^0-9a-fA-F\-+\*])")
    EXPECT_EQ(Pat4.match("1"), false);
    EXPECT_EQ(Pat4.match("*"), false);
    EXPECT_EQ(Pat4.match("^"), false);
    EXPECT_EQ(Pat4.match("s"), true);
    EXPECT_EQ(Pat4.match("S"), true);
    EXPECT_EQ(Pat4.match("H"), true);
    EXPECT_EQ(Pat4.match("]"), true);

    PATDEF(Pat5, R"([\*-\^])")
    EXPECT_EQ(Pat5.match("*"), true);
    EXPECT_EQ(Pat5.match("a"), false);
    EXPECT_EQ(Pat5.match("z"), false);
    EXPECT_EQ(Pat5.match("A"), true);
    EXPECT_EQ(Pat5.match("Z"), true);
    EXPECT_EQ(Pat5.match("\\"), true);
    EXPECT_EQ(Pat5.match("^"), true);
    EXPECT_EQ(Pat5.match("-"), true);

    PATDEF(Pat6, "foo.[^0-9]")
    EXPECT_EQ(Pat6.match("foo.5"), false);
    EXPECT_EQ(Pat6.match("foo.8"), false);
    EXPECT_EQ(Pat6.match("bar.5"), false);
    EXPECT_EQ(Pat6.match("foo.f"), true);

    PATDEF(Pat7, "foo.[!0-9]")
    EXPECT_EQ(Pat7.match("foo.5"), false);
    EXPECT_EQ(Pat7.match("foo.8"), false);
    EXPECT_EQ(Pat7.match("bar.5"), false);
    EXPECT_EQ(Pat7.match("foo.f"), true);

    PATDEF(Pat8, "foo.[0!^*?]")
    EXPECT_EQ(Pat8.match("foo.5"), false);
    EXPECT_EQ(Pat8.match("foo.8"), false);
    EXPECT_EQ(Pat8.match("foo.0"), true);
    EXPECT_EQ(Pat8.match("foo.!"), true);
    EXPECT_EQ(Pat8.match("foo.^"), true);
    EXPECT_EQ(Pat8.match("foo.*"), true);
    EXPECT_EQ(Pat8.match("foo.?"), true);

    PATDEF(Pat9, "foo[/]bar")
    EXPECT_EQ(Pat9.match("foo/bar"), false);

    PATDEF(Pat10, "foo.[[]")
    EXPECT_EQ(Pat10.match("foo.["), true);

    PATDEF(Pat11, "foo.[]]")
    EXPECT_EQ(Pat11.match("foo.]"), true);

    PATDEF(Pat12, "foo.[][!]")
    EXPECT_EQ(Pat12.match("foo.]"), true);
    EXPECT_EQ(Pat12.match("foo.["), true);
    EXPECT_EQ(Pat12.match("foo.!"), true);

    PATDEF(Pat13, "foo.[]-]")
    EXPECT_EQ(Pat13.match("foo.]"), true);
    EXPECT_EQ(Pat13.match("foo.-"), true);

    PATDEF(Pat14, "foo.[0-9]")
    EXPECT_EQ(Pat14.match("foo.5"), true);
    EXPECT_EQ(Pat14.match("foo.8"), true);
    EXPECT_EQ(Pat14.match("bar.5"), false);
    EXPECT_EQ(Pat14.match("foo.f"), false);

}

TEST(GlobPattern, BraceExpr) {
    PATDEF(Pat1, "*foo[0-9a-z].{c,cpp,cppm,?pp}")
    EXPECT_EQ(Pat1.match("foo1.cc"), false);
    EXPECT_EQ(Pat1.match("foo2.cpp"), true);
    EXPECT_EQ(Pat1.match("foo3.cppm"), true);
    EXPECT_EQ(Pat1.match("foot.cppm"), true);
    EXPECT_EQ(Pat1.match("foot.hpp"), true);
    EXPECT_EQ(Pat1.match("foot.app"), true);
    EXPECT_EQ(Pat1.match("fooD.cppm"), false);
    EXPECT_EQ(Pat1.match("BarfooD.cppm"), false);
    EXPECT_EQ(Pat1.match("foofooD.cppm"), false);

    PATDEF(Pat2, "proj/{build*,include,src}/*.{cc,cpp,h,hpp}")
    EXPECT_EQ(Pat2.match("proj/include/foo.cc"), true);
    EXPECT_EQ(Pat2.match("proj/include/bar.cpp"), true);
    EXPECT_EQ(Pat2.match("proj/include/xxx/yyy/zzz/foo.cc"), false);
    EXPECT_EQ(Pat2.match("proj/build-yyy/foo.h"), true);
    EXPECT_EQ(Pat2.match("proj/build-xxx/foo.cpp"), true);
    EXPECT_EQ(Pat2.match("proj/build/foo.cpp"), true);
    EXPECT_EQ(Pat2.match("proj/build-xxx/xxx/yyy/zzz/foo.cpp"), false);

    PATDEF(Pat3, "*.{html,js}")
    EXPECT_EQ(Pat3.match("foo.js"), true);
    EXPECT_EQ(Pat3.match("foo.html"), true);
    EXPECT_EQ(Pat3.match("folder/foo.js"), false);
    EXPECT_EQ(Pat3.match("/node_modules/foo.js"), false);
    EXPECT_EQ(Pat3.match("foo.jss"), false);
    EXPECT_EQ(Pat3.match("some.js/test"), false);

    PATDEF(Pat4, "*.{html}")
    EXPECT_EQ(Pat4.match("foo.html"), true);
    EXPECT_EQ(Pat4.match("foo.js"), false);
    EXPECT_EQ(Pat4.match("folder/foo.js"), false);
    EXPECT_EQ(Pat4.match("/node_modules/foo.js"), false);
    EXPECT_EQ(Pat4.match("foo.jss"), false);
    EXPECT_EQ(Pat4.match("some.js/test"), false);

    PATDEF(Pat5, "{node_modules,testing}")
    EXPECT_EQ(Pat5.match("node_modules"), true);
    EXPECT_EQ(Pat5.match("testing"), true);
    EXPECT_EQ(Pat5.match("node_module"), false);
    EXPECT_EQ(Pat5.match("dtesting"), false);

    PATDEF(Pat6, "**/{foo,bar}")
    EXPECT_EQ(Pat6.match("foo"), true);
    EXPECT_EQ(Pat6.match("bar"), true);
    EXPECT_EQ(Pat6.match("test/foo"), true);
    EXPECT_EQ(Pat6.match("test/bar"), true);
    EXPECT_EQ(Pat6.match("other/more/foo"), true);
    EXPECT_EQ(Pat6.match("other/more/bar"), true);
    EXPECT_EQ(Pat6.match("/foo"), true);
    EXPECT_EQ(Pat6.match("/bar"), true);
    EXPECT_EQ(Pat6.match("/test/foo"), true);
    EXPECT_EQ(Pat6.match("/test/bar"), true);
    EXPECT_EQ(Pat6.match("/other/more/foo"), true);
    EXPECT_EQ(Pat6.match("/other/more/bar"), true);

    PATDEF(Pat7, "{foo,bar}/**")
    EXPECT_EQ(Pat7.match("foo"), true);
    EXPECT_EQ(Pat7.match("bar"), true);
    EXPECT_EQ(Pat7.match("bar/"), true);
    EXPECT_EQ(Pat7.match("foo/test"), true);
    EXPECT_EQ(Pat7.match("bar/test"), true);
    EXPECT_EQ(Pat7.match("bar/test/"), true);
    EXPECT_EQ(Pat7.match("foo/other/more"), true);
    EXPECT_EQ(Pat7.match("bar/other/more"), true);
    EXPECT_EQ(Pat7.match("bar/other/more/"), true);

    PATDEF(Pat8, "{**/*.d.ts,**/*.js}")
    EXPECT_EQ(Pat8.match("foo.js"), true);
    EXPECT_EQ(Pat8.match("testing/foo.js"), true);
    EXPECT_EQ(Pat8.match("/testing/foo.js"), true);
    EXPECT_EQ(Pat8.match("foo.d.ts"), true);
    EXPECT_EQ(Pat8.match("testing/foo.d.ts"), true);
    EXPECT_EQ(Pat8.match("/testing/foo.d.ts"), true);
    EXPECT_EQ(Pat8.match("foo.d"), false);
    EXPECT_EQ(Pat8.match("testing/foo.d"), false);
    EXPECT_EQ(Pat8.match("/testing/foo.d"), false);

    PATDEF(Pat9, "{**/*.d.ts,**/*.js,path/simple.jgs}")
    EXPECT_EQ(Pat9.match("foo.js"), true);
    EXPECT_EQ(Pat9.match("testing/foo.js"), true);
    EXPECT_EQ(Pat9.match("/testing/foo.js"), true);
    EXPECT_EQ(Pat9.match("path/simple.jgs"), true);
    EXPECT_EQ(Pat9.match("/path/simple.jgs"), false);

    PATDEF(Pat10, "{**/*.d.ts,**/*.js,foo.[0-9]}")
    EXPECT_EQ(Pat10.match("foo.5"), true);
    EXPECT_EQ(Pat10.match("foo.8"), true);
    EXPECT_EQ(Pat10.match("bar.5"), false);
    EXPECT_EQ(Pat10.match("foo.f"), false);
    EXPECT_EQ(Pat10.match("foo.js"), true);

    PATDEF(Pat11, "prefix/{**/*.d.ts,**/*.js,foo.[0-9]}")
    EXPECT_EQ(Pat11.match("prefix/foo.5"), true);
    EXPECT_EQ(Pat11.match("prefix/foo.8"), true);
    EXPECT_EQ(Pat11.match("prefix/bar.5"), false);
    EXPECT_EQ(Pat11.match("prefix/foo.f"), false);
    EXPECT_EQ(Pat11.match("prefix/foo.js"), true);
}

TEST(GlobPattern, WildGlob) {
    PATDEF(Pat1, "**/*")
    EXPECT_EQ(Pat1.match("foo"), true);
    EXPECT_EQ(Pat1.match("foo/bar/baz"), true);

    PATDEF(Pat2, "**/[0-9]*")
    EXPECT_EQ(Pat2.match("114514foo"), true);
    EXPECT_EQ(Pat2.match("foo/bar/baz/xxx/yyy/zzz"), false);
    EXPECT_EQ(Pat2.match("foo/bar/baz/xxx/yyy/zzz114514"), false);
    EXPECT_EQ(Pat2.match("foo/bar/baz/xxx/yyy/114514"), true);
    EXPECT_EQ(Pat2.match("foo/bar/baz/xxx/yyy/114514zzz"), true);

    PATDEF(Pat3, "**/*[0-9]")
    EXPECT_EQ(Pat3.match("foo5"), true);
    EXPECT_EQ(Pat3.match("foo/bar/baz/xxx/yyy/zzz"), false);
    EXPECT_EQ(Pat3.match("foo/bar/baz/xxx/yyy/zzz114514"), true);

    PATDEF(Pat4, "**/include/test/*.{cc,hh,c,h,cpp,hpp}")
    EXPECT_EQ(Pat4.match("include/test/aaa.cc"), true);
    EXPECT_EQ(Pat4.match("/include/test/aaa.cc"), true);
    EXPECT_EQ(Pat4.match("xxx/yyy/include/test/aaa.cc"), true);
    EXPECT_EQ(Pat4.match("include/foo/bar/baz/include/test/bbb.hh"), true);
    EXPECT_EQ(Pat4.match("include/include/include/include/include/test/bbb.hpp"), true);

    PATDEF(Pat5, "**include/test/*.{cc,hh,c,h,cpp,hpp}")
    EXPECT_EQ(Pat5.match("include/test/fff.hpp"), true);
    EXPECT_EQ(Pat5.match("xxx-yyy-include/test/fff.hpp"), true);
    EXPECT_EQ(Pat5.match("xxx-yyy-include/test/.hpp"), true);
    EXPECT_EQ(Pat5.match("/include/test/aaa.cc"), true);
    EXPECT_EQ(Pat5.match("include/foo/bar/baz/include/test/bbb.hh"), true);

    PATDEF(Pat6, "**/*foo.{c,cpp}")
    EXPECT_EQ(Pat6.match("bar/foo.cpp"), true);
    EXPECT_EQ(Pat6.match("bar/barfoo.cpp"), true);
    EXPECT_EQ(Pat6.match("/foofoo.cpp"), true);
    EXPECT_EQ(Pat6.match("foo/foo/foo/foo/foofoo.cpp"), true);
    EXPECT_EQ(Pat6.match("foofoo.cpp"), true);
    EXPECT_EQ(Pat6.match("barfoo.cpp"), true);
    EXPECT_EQ(Pat6.match("foo.cpp"), true);

    // Boundary test of `**`
    PATDEF(Pat7, "**")
    EXPECT_EQ(Pat7.match("foo"), true);
    EXPECT_EQ(Pat7.match("foo/bar/baz"), true);

    PATDEF(Pat8, "x/**")
    EXPECT_EQ(Pat8.match("x/"), true);
    EXPECT_EQ(Pat8.match("x/foo/bar/baz"), true);
    EXPECT_EQ(Pat8.match("x"), true);

    PATDEF(Pat9, "**/x")
    EXPECT_EQ(Pat9.match("x"), true);
    EXPECT_EQ(Pat9.match("/x"), true);
    EXPECT_EQ(Pat9.match("/x/x/x/x/x"), true);

    PATDEF(Pat10, "**/*")
    EXPECT_EQ(Pat10.match("foo"), true);
    EXPECT_EQ(Pat10.match("foo/bar"), true);
    EXPECT_EQ(Pat10.match("foo/bar/baz"), true);

    PATDEF(Pat11, "**/*.{cc,cpp}")
    EXPECT_EQ(Pat11.match("foo/bar/baz.cc"), true);
    EXPECT_EQ(Pat11.match("foo/foo/foo.cpp"), true);
    EXPECT_EQ(Pat11.match("foo/bar/.cc"), true);

    PATDEF(Pat12, "**/*?.{cc,cpp}")
    EXPECT_EQ(Pat12.match("foo/bar/baz/xxx/yyy/zzz/aaa.cc"), true);
    EXPECT_EQ(Pat12.match("foo/bar/baz/xxx/yyy/zzz/a.cc"), true);
    EXPECT_EQ(Pat12.match("foo/bar/baz/xxx/yyy/zzz/.cc"), false);

    PATDEF(Pat13, "**/?*.{cc,cpp}")
    EXPECT_EQ(Pat13.match("foo/bar/baz/xxx/yyy/zzz/aaa.cc"), true);
    EXPECT_EQ(Pat13.match("foo/bar/baz/xxx/yyy/zzz/a.cc"), true);
    EXPECT_EQ(Pat13.match("foo/bar/baz/xxx/yyy/zzz/.cc"), false);

    PATDEF(Pat14, "**/*[0-9]")
    EXPECT_EQ(Pat14.match("foo5"), true);
    EXPECT_EQ(Pat14.match("foo/bar/baz/xxx/yyy/zzz"), false);
    EXPECT_EQ(Pat14.match("foo/bar/baz/xxx/yyy/zzz114514"), true);

    PATDEF(Pat15, "**/*")
    EXPECT_EQ(Pat15.match("foo"), true);
    EXPECT_EQ(Pat15.match("foo/bar/baz"), true);

    PATDEF(Pat16, "**/*.js")
    EXPECT_EQ(Pat16.match("foo.js"), true);
    EXPECT_EQ(Pat16.match("/foo.js"), true);
    EXPECT_EQ(Pat16.match("folder/foo.js"), true);
    EXPECT_EQ(Pat16.match("/node_modules/foo.js"), true);
    EXPECT_EQ(Pat16.match("foo.jss"), false);
    EXPECT_EQ(Pat16.match("some.js/test"), false);
    EXPECT_EQ(Pat16.match("/some.js/test"), false);

    PATDEF(Pat17, "**/project.json")
    EXPECT_EQ(Pat17.match("project.json"), true);
    EXPECT_EQ(Pat17.match("/project.json"), true);
    EXPECT_EQ(Pat17.match("some/folder/project.json"), true);
    EXPECT_EQ(Pat17.match("/some/folder/project.json"), true);
    EXPECT_EQ(Pat17.match("some/folder/file_project.json"), false);
    EXPECT_EQ(Pat17.match("some/folder/fileproject.json"), false);
    EXPECT_EQ(Pat17.match("some/rrproject.json"), false);

    PATDEF(Pat18, "test/**")
    EXPECT_EQ(Pat18.match("test"), true);
    EXPECT_EQ(Pat18.match("test/foo"), true);
    EXPECT_EQ(Pat18.match("test/foo/"), true);
    EXPECT_EQ(Pat18.match("test/foo.js"), true);
    EXPECT_EQ(Pat18.match("test/other/foo.js"), true);
    EXPECT_EQ(Pat18.match("est/other/foo.js"), false);

    PATDEF(Pat19, "**")
    EXPECT_EQ(Pat19.match("/"), true);
    EXPECT_EQ(Pat19.match("foo.js"), true);
    EXPECT_EQ(Pat19.match("folder/foo.js"), true);
    EXPECT_EQ(Pat19.match("folder/foo/"), true);
    EXPECT_EQ(Pat19.match("/node_modules/foo.js"), true);
    EXPECT_EQ(Pat19.match("foo.jss"), true);
    EXPECT_EQ(Pat19.match("some.js/test"), true);

    PATDEF(Pat20, "test/**/*.js")
    EXPECT_EQ(Pat20.match("test/foo.js"), true);
    EXPECT_EQ(Pat20.match("test/other/foo.js"), true);
    EXPECT_EQ(Pat20.match("test/other/more/foo.js"), true);
    EXPECT_EQ(Pat20.match("test/foo.ts"), false);
    EXPECT_EQ(Pat20.match("test/other/foo.ts"), false);
    EXPECT_EQ(Pat20.match("test/other/more/foo.ts"), false);

    PATDEF(Pat21, "**/**/*.js")
    EXPECT_EQ(Pat21.match("foo.js"), true);
    EXPECT_EQ(Pat21.match("/foo.js"), true);
    EXPECT_EQ(Pat21.match("folder/foo.js"), true);
    EXPECT_EQ(Pat21.match("/node_modules/foo.js"), true);
    EXPECT_EQ(Pat21.match("foo.jss"), false);
    EXPECT_EQ(Pat21.match("some.js/test"), false);

    PATDEF(Pat22, "**/node_modules/**/*.js")
    EXPECT_EQ(Pat22.match("foo.js"), false);
    EXPECT_EQ(Pat22.match("folder/foo.js"), false);
    EXPECT_EQ(Pat22.match("node_modules/foo.js"), true);
    EXPECT_EQ(Pat22.match("/node_modules/foo.js"), true);
    EXPECT_EQ(Pat22.match("node_modules/some/folder/foo.js"), true);
    EXPECT_EQ(Pat22.match("/node_modules/some/folder/foo.js"), true);
    EXPECT_EQ(Pat22.match("node_modules/some/folder/foo.ts"), false);
    EXPECT_EQ(Pat22.match("foo.jss"), false);
    EXPECT_EQ(Pat22.match("some.js/test"), false);

    PATDEF(Pat23, "{**/node_modules/**,**/.git/**,**/bower_components/**}")
    EXPECT_EQ(Pat23.match("node_modules"), true);
    EXPECT_EQ(Pat23.match("/node_modules"), true);
    EXPECT_EQ(Pat23.match("/node_modules/more"), true);
    EXPECT_EQ(Pat23.match("some/test/node_modules"), true);
    EXPECT_EQ(Pat23.match("/some/test/node_modules"), true);
    EXPECT_EQ(Pat23.match("bower_components"), true);
    EXPECT_EQ(Pat23.match("bower_components/more"), true);
    EXPECT_EQ(Pat23.match("/bower_components"), true);
    EXPECT_EQ(Pat23.match("some/test/bower_components"), true);
    EXPECT_EQ(Pat23.match("/some/test/bower_components"), true);
    EXPECT_EQ(Pat23.match(".git"), true);
    EXPECT_EQ(Pat23.match("/.git"), true);
    EXPECT_EQ(Pat23.match("some/test/.git"), true);
    EXPECT_EQ(Pat23.match("/some/test/.git"), true);
    EXPECT_EQ(Pat23.match("tempting"), false);
    EXPECT_EQ(Pat23.match("/tempting"), false);
    EXPECT_EQ(Pat23.match("some/test/tempting"), false);
    EXPECT_EQ(Pat23.match("/some/test/tempting"), false);

    PATDEF(Pat24, "{**/package.json,**/project.json}")
    EXPECT_EQ(Pat24.match("package.json"), true);
    EXPECT_EQ(Pat24.match("/package.json"), true);
    EXPECT_EQ(Pat24.match("xpackage.json"), false);
    EXPECT_EQ(Pat24.match("/xpackage.json"), false);

    PATDEF(Pat25, "some/**/*.js")
    EXPECT_EQ(Pat25.match("some/foo.js"), true);
    EXPECT_EQ(Pat25.match("some/folder/foo.js"), true);
    EXPECT_EQ(Pat25.match("something/foo.js"), false);
    EXPECT_EQ(Pat25.match("something/folder/foo.js"), false);

    PATDEF(Pat26, "some/**/*")
    EXPECT_EQ(Pat26.match("some/foo.js"), true);
    EXPECT_EQ(Pat26.match("some/folder/foo.js"), true);
    EXPECT_EQ(Pat26.match("something/foo.js"), false);
    EXPECT_EQ(Pat26.match("something/folder/foo.js"), false);
}

}  // namespace clice::testing
