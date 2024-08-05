int x = 1;
static_assert(__is_same(int, int));
auto y = new int(1);
#if 0
template <typename T>
struct A {
    template <typename T2>
    struct B {
        template <typename T3>
        struct C {
            using type = type_list<T, T2, T3>;
        };
    };
};

template <typename U, typename U2, typename U3>
struct Test {
    using result = typename A<U>::template B<U2>::template C<U3>::type;
};

#elif 0
template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename T2>
struct B {
    using type = typename A<T2>::type;
};

template <typename T3>
struct C {
    using type = typename B<T3>::type;
};

template <typename U>
struct Test {
    using result = typename C<U>::type;
};

#elif 0
template <typename T>
struct A {
    using type = type_list<T>;
};

template <typename U>
struct B {
    using type = typename U::type;
};

template <typename V, typename Z = A<V>>
struct Test {
    using result = typename B<Z>::type;
};

#elif 0
// #include <vector>

template <typename T>
struct A {
    template <typename T2>
    using type = type_list<T, T2>;
};

template <typename U1>
struct B {
    using base = A<U1>;

    template <typename U2>
    struct rebind {
        using other = typename base::template type<U2>;
    };
};

// #include <vector>
template <typename V1, typename V2>
struct Test {
    // using result = typename std::_Vector_base<V, std::allocator<V>>::_Tp_alloc_type;
    // using result = typename __gnu_cxx::__alloc_traits<std::allocator<V>>::template rebind<V>::other;
    using result = typename B<V1>::template rebind<V2>::other;
};

#elif 0

// #include <type_traits>

template <typename T, typename = void>
struct X {
    using type = type_list<char, T>;
};

template <typename T, template <typename...> typename List>
struct X<List<T>> {
    using type = type_list<int, T>;
};

template <typename V>
struct Result {
    using result = typename X<type_list<V>>::type;
};

X<type_list<int>>::type x;

#elif 1

// #include <bits/allocator.h>
// #include <vector>
//
// template <typename V>
// struct Test {
//     // using type = typename X<X<N>>::type;
//     // using result = typename std::_Vector_base<U1, std::allocator<U1>>::_Tp_alloc_type;
//     // using result = typename __gnu_cxx::__alloc_traits<std::allocator<V>>::template rebind<V>::other;
//     // using result = std::allocator_traits<std::allocator<V>>::template rebind_alloc<V>;
//     using result = std::__allocator_traits_base::template __rebind<std::allocator<V>, V>::type;
//     // using result = std::vector<U1>::reference;
//     //  using type = typename X<N>::template Y<U>::type2;
//     //  using result = typename A<U1>::template B<U2>::template C<U3>::type;
//     //  using type = decltype(A<U1>::type::template f<int>());
// };
//
// std::__allocator_traits_base::template __rebind<std::allocator<int>, int>::type x;

#endif

// int x = 1 + 2;
