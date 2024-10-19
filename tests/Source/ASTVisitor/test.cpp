void g();

void f() {
    int x;
    auto f2 = [x]() {
    };
    f2();
    g();
}
