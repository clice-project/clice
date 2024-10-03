template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    template <typename T2>
    struct B {
        template <typename T3>
        struct C {
            using type = type_list<T1, T2, T3>;
        };
    };
};

template <typename X, typename Y, typename Z>
struct test {
    using result = typename A<X>::template B<Y>::template C<Z>::type;
    using expect = type_list<X, Y, Z>;
};
