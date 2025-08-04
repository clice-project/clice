#include "Test/Tester.h"
#include "AST/Selection.h"

namespace clice::testing {

void EXPECT_SELECT(llvm::StringRef code, const char* kind, LocationChain chain = LocationChain()) {
    Tester tester;
    tester.add_main("main.cpp", code);
    ASSERT_TRUE(tester.compile(), chain);

    auto points = tester.nameless_points();
    ASSERT_TRUE(points.size() >= 1, chain);

    LocalSourceRange selected_range;
    selected_range.begin = points[0];
    selected_range.end = points.size() == 2 ? points[1] : points[0];

    auto tree = SelectionTree::createRight(*tester.unit, selected_range);

    auto node = tree.commonAncestor();
    if(!kind) {
        ASSERT_FALSE(node, chain);
    } else {
        ASSERT_TRUE(node, chain);
        auto [fid, range] = tester.unit->decompose_range(node->ASTNode.getSourceRange());
        llvm::outs() << tree << "\n";
        ASSERT_EQ(node->kind(), llvm::StringRef(kind), chain);
        ASSERT_EQ(range, tester.range(), chain);
    }
}

TEST(Selection, Expressions) {
    EXPECT_SELECT(R"(
          struct AAA { struct BBB { static int ccc(); };};
          int x = @[AAA::BBB::c$c$c]();
          )",
                  "DeclRefExpr");

    EXPECT_SELECT(R"(
          struct AAA { struct BBB { static int ccc(); };};
          int x = @[AAA::BBB::ccc($)];
          )",
                  "CallExpr");

    EXPECT_SELECT(R"(
          struct S {
            int foo() const;
            int bar() { return @[f$oo](); }
          };
          )",
                  "MemberExpr");

    EXPECT_SELECT(R"(void foo() { @[$foo](); })", "DeclRefExpr");
    EXPECT_SELECT(R"(void foo() { @[f$oo](); })", "DeclRefExpr");
    EXPECT_SELECT(R"(void foo() { @[fo$o](); })", "DeclRefExpr");

    EXPECT_SELECT(R"(void foo() { @[foo$] (); })", "DeclRefExpr");

    EXPECT_SELECT(R"(void foo() { @[foo$()]; })", "CallExpr");
    EXPECT_SELECT(R"(void foo() { @[foo$()]; /*comment*/$})", "CallExpr");
    EXPECT_SELECT(R"(const int x = 1, y = 2; int array[ @[$x] ][10][y];)", "DeclRefExpr");
    EXPECT_SELECT(R"(const int x = 1, y = 2; int array[x][10][ @[$y] ];)", "DeclRefExpr");
    EXPECT_SELECT(R"(void func(int x) { int v_array[ @[$x] ][10]; })", "DeclRefExpr");
    EXPECT_SELECT(R"(
        int a;
        decltype(@[$a] + a) b;
        )",
                  "DeclRefExpr");

    EXPECT_SELECT(R"(
        void func() { @[__$func__]; }
        )",
                  "PredefinedExpr");
}

TEST(Selection, Literals) {
    EXPECT_SELECT(R"(
          auto lambda = [](const char*){ return 0; };
          int x = lambda(@["y$"]);
          )",
                  "StringLiteral");

    EXPECT_SELECT(R"(int x = @[42]$;)", "IntegerLiteral");
    EXPECT_SELECT(R"(const int x = 1, y = 2; int array[x][ @[$10] ][y];)", "IntegerLiteral");

    EXPECT_SELECT(R"(
          struct Foo{};
          Foo operator""_ud(unsigned long long);
          Foo x = @[$12_ud];
          )",
                  "UserDefinedLiteral");
}

TEST(Selection, ControlFlow) {
    EXPECT_SELECT(R"(
          void foo() { @[if (1$11) { return; } else {$ }]} }
          )",
                  "IfStmt");

    EXPECT_SELECT(R"(int bar; void foo() @[{ foo (); }]$)", "CompoundStmt");

    /// FIXME:
    /// EXPECT_SELECT(R"(
    ///     /*error-ok*/
    ///     void func() @[{^])",
    ///               "CompoundStmt");

    EXPECT_SELECT(R"(
          struct Str {
            const char *begin();
            const char *end();
          };
          Str makeStr(const char*);
          void loop() {
            for (const char C : @[mak$eStr("foo"$)])
              ;
          }
          )",
                  "CallExpr");
}

TEST(Selection, Declarations) {
    /// FIXME: how to handle this?
    /// EXPECT_SELECT(R"(
    ///      #define TARGET void foo()
    ///      @[TAR$GET{ return; }]
    ///      )",
    ///              "FunctionDecl");

    EXPECT_SELECT(R"(@[$void foo$()];)", "FunctionDecl");
    EXPECT_SELECT(R"(@[void $foo()];)", "FunctionDecl");

    EXPECT_SELECT(R"(
          struct S { S(const char*); };
          @[S s $= "foo"];
          )",
                  "VarDecl");

    EXPECT_SELECT(R"(
          struct S { S(const char*); };
          @[S $s = "foo"];
          )",
                  "VarDecl");

    EXPECT_SELECT(R"(
          @[void (*$S)(int) = nullptr];
          )",
                  "VarDecl");

    EXPECT_SELECT(R"(@[int $a], b;)", "VarDecl");
    EXPECT_SELECT(R"(@[int a, $b];)", "VarDecl");
    EXPECT_SELECT(R"(@[struct {int x;} $y];)", "VarDecl");
    EXPECT_SELECT(R"(struct foo { @[int has$h<:32:>]; };)", "FieldDecl");
    EXPECT_SELECT(R"(struct {@[int $x];} y;)", "FieldDecl");

    EXPECT_SELECT(R"(
        void test(int bar) {
          auto l = [ $@[foo = bar] ] { };
        })",
                  "VarDecl");
}

TEST(Selection, Types) {
    EXPECT_SELECT(R"(
          struct AAA { struct BBB { static int ccc(); };};
          int x = AAA::@[B$B$B]::ccc();
          )",
                  "RecordTypeLoc");
    EXPECT_SELECT(R"(
          struct AAA { struct BBB { static int ccc(); };};
          int x = AAA::@[B$BB$]::ccc();
          )",
                  "RecordTypeLoc");
    EXPECT_SELECT(R"(
          struct Foo {};
          struct Bar : private @[Fo$o] {};
          )",
                  "RecordTypeLoc");
    EXPECT_SELECT(R"(
          struct Foo {};
          struct Bar : @[Fo$o] {};
          )",
                  "RecordTypeLoc");
    EXPECT_SELECT(R"(@[$void] (*S)(int) = nullptr;)", "BuiltinTypeLoc");
    /// EXPECT_SELECT(R"(@[void (*S)$(int)] = nullptr;)", "FunctionProtoTypeLoc");
    EXPECT_SELECT(R"(@[void ($*S)(int)] = nullptr;)", "PointerTypeLoc");
    /// EXPECT_SELECT(R"(@[void $(*S)(int)] = nullptr;)", "ParenTypeLoc");
    EXPECT_SELECT(R"(@[$void] foo();)", "BuiltinTypeLoc");
    EXPECT_SELECT(R"(@[void foo$()];)", "FunctionProtoTypeLoc");
    EXPECT_SELECT(R"(const int x = 1, y = 2; @[i$nt] array[x][10][y];)", "BuiltinTypeLoc");
    EXPECT_SELECT(R"(int (*getFunc(@[do$uble]))(int);)", "BuiltinTypeLoc");
    EXPECT_SELECT(R"(class X{}; @[int X::$*]y[10];)", "MemberPointerTypeLoc");
    EXPECT_SELECT(R"(const @[a$uto] x = 42;)", "AutoTypeLoc");
    /// EXPECT_SELECT(R"(@[decltype$(1)] b;)", "DecltypeTypeLoc");
    EXPECT_SELECT(R"(@[de$cltype(a$uto)] a = 1;)", "AutoTypeLoc");
    EXPECT_SELECT(R"(
        typedef int Foo;
        enum Bar : @[Fo$o] {};
      )",
                  "TypedefTypeLoc");
    EXPECT_SELECT(R"(
        typedef int Foo;
        enum Bar : @[Fo$o];
      )",
                  "TypedefTypeLoc");
}

TEST(Selection, CXXFeatures) {
    EXPECT_SELECT(R"(
          template <typename T>
          int x = @[T::$U::]ccc();
          )",
                  "NestedNameSpecifierLoc");
    EXPECT_SELECT(R"(
          struct Foo {};
          struct Bar : @[v$ir$tual private Foo] {};
          )",
                  "CXXBaseSpecifier");
    EXPECT_SELECT(R"(
          struct X { X(int); };
          class Y {
            X x;
            Y() : @[$x(4)] {}
          };
          )",
                  "CXXCtorInitializer");
    EXPECT_SELECT(R"(@[st$ruct {int x;}] y;)", "CXXRecordDecl");
    EXPECT_SELECT(R"(struct foo { @[op$erator int()]; };)", "CXXConversionDecl");
    EXPECT_SELECT(R"(struct foo { @[$~foo()]; };)", "CXXDestructorDecl");
    EXPECT_SELECT(R"(struct foo { @[~$foo()]; };)", "CXXDestructorDecl");
    EXPECT_SELECT(R"(struct foo { @[fo$o(){}] };)", "CXXConstructorDecl");
    EXPECT_SELECT(R"(
        struct S1 { void f(); };
        struct S2 { S1 * operator->(); };
        void test(S2 s2) {
          s2@[-$>]f();
        }
      )",
                  "DeclRefExpr");  // Test for overloaded operator->
}

TEST(Selection, UsingEnum) {
    EXPECT_SELECT(R"(
        namespace ns { enum class A {}; };
        using enum ns::@[$A];
        )",
                  "EnumTypeLoc");
    EXPECT_SELECT(R"(
        namespace ns { enum class A {}; using B = A; };
        using enum ns::@[$B];
        )",
                  "TypedefTypeLoc");
    EXPECT_SELECT(R"(
        namespace ns { enum class A {}; };
        using enum @[$ns::]A;
        )",
                  "NestedNameSpecifierLoc");
    EXPECT_SELECT(R"(
        namespace ns { enum class A {}; };
        @[using $enum ns::A];
        )",
                  "UsingEnumDecl");
    EXPECT_SELECT(R"(
        namespace ns { enum class A {}; };
        @[$using enum ns::A];
        )",
                  "UsingEnumDecl");
}

TEST(Selection, Templates) {
    EXPECT_SELECT(R"(template<typename ...T> void foo(@[T*$...]x);)", "PackExpansionTypeLoc");
    EXPECT_SELECT(R"(template<typename ...T> void foo(@[$T]*...x);)", "TemplateTypeParmTypeLoc");
    EXPECT_SELECT(R"(template <typename T> void foo() { @[$T] t; })", "TemplateTypeParmTypeLoc");
    EXPECT_SELECT(R"(
          template <class T> struct Foo {};
          template <@[template<class> class /*cursor here*/$U]>
            struct Foo<U<int>*> {};
          )",
                  "TemplateTemplateParmDecl");
    EXPECT_SELECT(R"(template <class T> struct foo { ~foo<@[$T]>(){} };)",
                  "TemplateTypeParmTypeLoc");
    EXPECT_SELECT(R"(
        template <typename> class Vector {};
        template <template <typename> class Container> class A {};
        A<@[V$ector]> a;
      )",
                  "TemplateArgumentLoc");
}

TEST(Selection, Concepts) {
    EXPECT_SELECT(R"(
        template <class> concept C = true;
        auto x = @[$C<int>];
      )",
                  "ConceptReference");
    EXPECT_SELECT(R"(
        template <class> concept C = true;
        @[$C] auto x = 0;
      )",
                  "ConceptReference");
    EXPECT_SELECT(R"(
        template <class> concept C = true;
        void foo(@[$C] auto x) {}
      )",
                  "ConceptReference");
    EXPECT_SELECT(R"(
        template <class> concept C = true;
        template <@[$C] x> int i = 0;
      )",
                  "ConceptReference");
    EXPECT_SELECT(R"(
        namespace ns { template <class> concept C = true; }
        auto x = @[ns::$C<int>];
      )",
                  "ConceptReference");
    EXPECT_SELECT(R"(
        template <typename T, typename K>
        concept D = true;
        template <typename T> void g(D<@[$T]> auto abc) {}
      )",
                  "TemplateTypeParmTypeLoc");
}

TEST(Selection, Attributes) {
    EXPECT_SELECT(R"(
        void f(int * __attribute__((@[no$nnull])) );
      )",
                  "NonNullAttr");
    EXPECT_SELECT(R"(
        // Digraph syntax for attributes to avoid accidental annotations.
        class [[gsl::Owner( @[in$t] )]] X{};
      )",
                  "BuiltinTypeLoc");
}

/// TEST(Selection, Macros) {
///     EXPECT_SELECT(R"(
///           int x(int);
///           #define M(foo) x(foo)
///           int a = 42;
///           int b = M(@[$a]);
///           )",
///                   "DeclRefExpr");
///     EXPECT_SELECT(R"(
///           void foo();
///           #define CALL_FUNCTION(X) X()
///           void bar() { CALL_FUNCTION(@[f$o$o]); }
///           )",
///                   "DeclRefExpr");
///     EXPECT_SELECT(R"(
///           void foo();
///           #define CALL_FUNCTION(X) X()
///           void bar() { @[CALL_FUNC$TION(fo$o)]; }
///           )",
///                   "CallExpr");
///     EXPECT_SELECT(R"(
///           void foo();
///           #define CALL_FUNCTION(X) X()
///           void bar() { @[C$ALL_FUNC$TION(foo)]; }
///           )",
///                   "CallExpr");
/// }

/// TEST(Selection, NullOrInvalid) {
///     EXPECT_SELECT(R"(
///           void foo();
///           #$define CALL_FUNCTION(X) X($)
///           void bar() { CALL_FUNCTION(foo); }
///           )",
///                   nullptr);
///     EXPECT_SELECT(R"(
///           void foo();
///           #define CALL_FUNCTION(X) X()
///           void bar() { CALL_FUNCTION(foo$)$; }
///           )",
///                   nullptr);
///     EXPECT_SELECT(R"(
///           namespace ns {
///           #if 0
///           void fo$o() {}
///           #endif
///           }
///           )",
///                   nullptr);
///     EXPECT_SELECT(R"(co$nst auto x = 42;)", nullptr);
///     EXPECT_SELECT(R"($)", nullptr);
///     EXPECT_SELECT(R"(int x = 42;$)", nullptr);
///     EXPECT_SELECT(R"($int x; int y;$)", nullptr);
///     EXPECT_SELECT(R"(void foo() { @[foo$$] (); })",
///                   "DeclRefExpr");  // Technically valid, but tricky
/// }

TEST(Selection, InjectedClassName) {}

TEST(Selection, Metrics) {}

TEST(Selection, Selected) {}

TEST(Selection, PathologicalPreprocessor) {}

TEST(SelectionTest, IncludedFile) {}

TEST(SelectionTest, MacroArgExpansion) {}

TEST(SelectionTest, Implicit) {}

TEST(SelectionTest, DeclContextIsLexical) {}

TEST(SelectionTest, DeclContextLambda) {}

TEST(SelectionTest, UsingConcepts) {}

}  // namespace clice::testing
