template <typename T>
int foo(T) {}

template <typename T>
void bar(T t) {
    auto x = foo(t);
}
