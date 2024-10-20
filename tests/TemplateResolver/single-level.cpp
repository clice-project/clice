template <typename... Ts>
struct type_list {};

template <typename T>
struct A {
    using type = type_list<T>;
};

template <typename X>
struct test {
    using result = typename A<X>::type;
    using expect = type_list<X>;
};
