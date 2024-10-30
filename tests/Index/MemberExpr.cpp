namespace {

struct Foo {
    int bar;
};

Foo foo = {};

int x = foo.bar;
//          ^^^ MemberExpr

}  // namespace
