template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {};

template <typename U2>
struct B {};

template <typename U2, template <typename...> typename HKT>
struct B<HKT<U2>> {
    using type = type_list<U2>;
};

template <typename X>
struct test {
    using result = typename B<A<X>>::type;
    using expect = type_list<X>;
};
