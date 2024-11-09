template <typename T>
void foo() {
    typename T::type::type::type x;
    T::x = 1;
    T::x.y = 2;
    typename T::type::type::name y;
}
