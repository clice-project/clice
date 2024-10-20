template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using self = A<T1>;
    using type = type_list<T1>;
};

template <typename X>
struct test {
    using result = typename A<X>::self::self::self::self::self::type;
    using expect = type_list<X>;
};
