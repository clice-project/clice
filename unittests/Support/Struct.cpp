#include "Test.h"

namespace clice {

namespace {

struct X {
    int x;
    int y;
};

static_assert(std::is_same_v<support::member_types<X>, type_list<int, int>>);

TEST(Support, Struct) {
    bool x = false, y = false;
    support::foreach(X{1, 2}, [&](auto name, auto value) {
        if(name == "x") {
            x = true;
            EXPECT_EQ(value, 1);
        } else if(name == "y") {
            y = true;
            EXPECT_EQ(value, 2);
        } else {
            EXPECT_TRUE(false);
        }
    });
    EXPECT_TRUE(x && y);

    struct X x1 = {1, 2};
    struct X x2 = {3, 4};
    EXPECT_TRUE(support::foreach(x1, x2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
    EXPECT_EQ(x1.x, 3);
    EXPECT_EQ(x1.y, 4);
}

inherited_struct(Y, X) {
    int z;
};

static_assert(std::is_same_v<support::member_types<Y>, type_list<int, int, int>>);

TEST(Support, Inheritance) {
    bool x = false, y = false, z = false;
    support::foreach(Y{1, 2, 3}, [&](auto name, auto value) {
        if(name == "x") {
            x = true;
            EXPECT_EQ(value, 1);
        } else if(name == "y") {
            y = true;
            EXPECT_EQ(value, 2);
        } else if(name == "z") {
            z = true;
            EXPECT_EQ(value, 3);
        } else {
            EXPECT_TRUE(false);
        }
    });
    EXPECT_TRUE(x && y && z);
}

}  // namespace

}  // namespace clice

