template <typename T, typename U>
struct foo {};

template <typename T>
struct foo<T, T> {};

template <>
struct foo<int, int> {};

template struct foo<char, int>;

template struct foo<char, char>;

foo<int, int> a;
foo<int, char> b;
foo<char, int> c;
foo<char, char> d;
