template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename U2>
struct B : A<U2> {};

template <typename X>
struct test {
    using result = typename B<X>::type;
    using expect = type_list<X>;
};
