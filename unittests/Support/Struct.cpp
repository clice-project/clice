#include "gtest/gtest.h"
#include "Support/Struct.h"
#include "Support/JSON.h"

namespace clice {

namespace {

struct X {
    int x;
    int y;

    friend constexpr bool operator== (const X& lhs, const X& rhs) noexcept = default;
};

static_assert(std::is_same_v<refl::member_types<X>, type_list<int, int>>);

TEST(Support, Struct) {
    bool x = false, y = false;
    refl::foreach(X{1, 2}, [&](auto name, auto value) {
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

    X x1 = {1, 2};
    X x2 = {3, 4};
    EXPECT_TRUE(refl::foreach(x1, x2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
    EXPECT_EQ(x1.x, 3);
    EXPECT_EQ(x1.y, 4);

    auto j1 = json::Value(json::Object{
        {"x", 3},
        {"y", 4},
    });
    EXPECT_EQ(json::serialize(x1), j1);

    auto j2 = json::Value(json::Object{
        {"x", 3},
        {"y", 4},
    });
    EXPECT_EQ(x2, json::deserialize<X>(j2));
}

inherited_struct(Y, X) {
    int z;
};

static_assert(std::is_same_v<refl::member_types<Y>, type_list<int, int, int>>);

TEST(Support, Inheritance) {
    bool x = false, y = false, z = false;
    refl::foreach(Y{1, 2, 3}, [&](auto name, auto value) {
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

    Y y1 = {1, 2, 3};
    Y y2 = {4, 5, 6};
    EXPECT_TRUE(refl::foreach(y1, y2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
    EXPECT_EQ(y1.x, 4);
    EXPECT_EQ(y1.y, 5);
    EXPECT_EQ(y1.z, 6);

    auto j1 = json::Value(json::Object{
        {"x", 4},
        {"y", 5},
        {"z", 6},
    });
    EXPECT_EQ(json::serialize(y1), j1);

    auto j2 = json::Value(json::Object{
        {"x", 4},
        {"y", 5},
        {"z", 6},
    });
    auto y3 = json::deserialize<Y>(j2);
    EXPECT_EQ(y2.x, y3.x);
    EXPECT_EQ(y2.y, y3.y);
    EXPECT_EQ(y2.z, y3.z);
}

}  // namespace

}  // namespace clice

