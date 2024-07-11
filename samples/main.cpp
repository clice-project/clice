#define EXPAND(...) __VA_ARGS__
#define GET_PARAM(...) __VA_ARGS__,
#define GET_TYPE_IMPL(...) GET_PARAM __VA_ARGS__

#define GET_FST(_1, ...) _1
#define GET_WRAP(...) GET_FST(__VA_ARGS__)
#define GET_TYPE(x) EXPAND(GET_WRAP(GET_TYPE_IMPL(x)))

int main() {
    GET_TYPE((int)y) x = 1;
    GET_TYPE((double)y) y = 1;
    return 0;
}
