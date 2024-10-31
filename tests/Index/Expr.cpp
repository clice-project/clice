namespace {

struct Foo {
    int bar;
};

Foo foo = {};

int x = foo.bar;

//          ^^^ MemberExpr

template <typename T>
void bar(T t) {
    t.x.x();
}

namespace X {
int yyy;
};

using namespace X;

void foo2() {
    yyy = 2;
}

}  // namespace
