#include <utility>  
#include <vector>
#include <string>

template <typename T>
struct X {
    void foo();
};

int main() {
    X<int> x;
    x.foo();
}
