#include <vector>

template <typename T>
struct test {
    using result = typename std::vector<T>::reference;
    using expect = T&;
};
