#include <iostream>
#include <tuple>
using Callback = void (*)();

void register_(Callback fn);

template <typename T,
          auto seed =
              [] {
              }>
struct state {
    inline static T value;
};

std::size_t index2 = 0;

int main() {
    int x = 3;
    auto fn1 = []() {
        std::cout << "hello\n";
    };

    using State1 = state<void*>;
    State1::value = &fn1;

    register_([]() {
        auto fn = reinterpret_cast<Callback>(State1::value);
        fn();
    });

    using State2 = state<int>;
    State2::value = x;

    register_([]() {
        std::cout << State2::value << "\n";
    });
}
