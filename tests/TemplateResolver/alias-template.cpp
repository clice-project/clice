template <typename... Ts>
struct type_list {};

template <typename T1, typename U1>
struct A {
    using type = type_list<T1, U1>;
};

template <typename T2>
struct B {
    template <typename U2>
    using type = typename A<T2, U2>::type;
};

template <typename X, typename Y>
struct test {
    using result = typename B<X>::template type<Y>;
    using expect = type_list<X, Y>;
};
