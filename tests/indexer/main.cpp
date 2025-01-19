#include "foo.h"

#include "macro.h"
#define TEST
#include "macro.h"

int main() {
    int v = foo();
    A a = {};
    B b = {};
    return 0;
}
