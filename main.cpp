template <typename T>
struct A {
    using reference = T&;
};

template <typename U>
struct B {
    using reference = A<U>::reference;
};

template <typename Z>
struct C {
    using type = B<Z>::reference;
};
