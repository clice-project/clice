#include <type_traits>

template <typename T>
std::enable_if_t<!std::is_same_v<T, int>, int> foo() {}

template <typename T>
std::enable_if_t<!std::is_same_v<T, double>, int> foo() {}

int foo();

int main() {
    foo();
    foo<int>();
    foo<double>();
    return 0;
}
