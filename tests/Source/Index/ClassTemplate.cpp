template <typename T, typename U>
struct X {};

template <typename T>
struct X<T, T> {};

template <>
struct X<int, int> {};

X<int, char> x;

X<char, char> x2;

X<int, int> x3;

template <>
struct X<double, char>;
