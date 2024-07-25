#include <iostream>

#define X(x)                                                                                                 \
    int x;                                                                                                   \
    int y;                                                                                                   \
    int z;

X(x)

int main() {
    std::cout << x << std::endl;
    return 0;
}
