#define TEST(name)                                                                                 \
    struct name {};                                                                                \
    struct name2 {};                                                                               \
    struct name##3 {};                                                                             \
    struct name name;

TEST(name);

int main() {
    struct name x;
    struct name2 y;
    struct name3 z;
}

