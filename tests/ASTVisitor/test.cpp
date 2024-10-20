template <typename T, typename U>
struct X {
    using type = char;
};

template <typename T>
struct X<T, T> {
    using type = int;
};

void f() {
    typename X<char, int>::type y;
    typename X<int, int>::type x;
}
