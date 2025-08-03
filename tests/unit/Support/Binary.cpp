#include "Test/Tester.h"
#include "Support/Binary.h"

namespace clice::testing {

namespace {

template <typename Object, typename... Ts>
constexpr inline bool check_sections =
    std::is_same_v<binary::layout_t<Object>, std::tuple<binary::Section<Ts>...>>;

TEST(Binary, String) {
    static_assert(check_sections<std::string, char>);

    {
        std::string s1 = "";
        auto [buffer, proxy] = binary::serialize(s1);
        EXPECT_EQ(s1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(s1[i], proxy[i].value());
        }

        EXPECT_EQ(s1, proxy.as_string());
        std::string s2 = binary::deserialize(proxy);
        EXPECT_EQ(s1, s2);
    }

    {
        std::string s1 = "123";
        auto [buffer, proxy] = binary::serialize(s1);
        EXPECT_EQ(s1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(s1[i], proxy[i].value());
        }

        EXPECT_EQ(s1, proxy.as_string());
        std::string s2 = binary::deserialize(proxy);
        EXPECT_EQ(s1, s2);
    }

    {
        std::string s1 = "11111111111111111111111111111111111111111111111111111111111111111";
        auto [buffer, proxy] = binary::serialize(s1);
        EXPECT_EQ(s1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(s1[i], proxy[i].value());
        }

        EXPECT_EQ(s1, proxy.as_string());
        std::string s2 = binary::deserialize(proxy);
        EXPECT_EQ(s1, s2);
    }
}

TEST(Binary, Array) {
    static_assert(check_sections<std::vector<int>, int>);

    {
        std::vector<int> vec1 = {};
        auto [buffer, proxy] = binary::serialize(vec1);
        EXPECT_EQ(vec1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(vec1[i], proxy[i].value());
        }

        EXPECT_EQ(vec1, proxy.as_array().vec());
        std::vector vec2 = binary::deserialize(proxy);
        EXPECT_EQ(vec1, vec2);
    }

    {
        std::vector vec1 = {1, 2, 3};
        auto [buffer, proxy] = binary::serialize(vec1);
        EXPECT_EQ(vec1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(vec1[i], proxy[i].value());
        }

        EXPECT_EQ(vec1, proxy.as_array().vec());
        std::vector vec2 = binary::deserialize(proxy);
        EXPECT_EQ(vec1, vec2);
    }

    {
        std::vector vec1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto [buffer, proxy] = binary::serialize(vec1);
        EXPECT_EQ(vec1.size(), proxy.size());
        for(auto i = 0; i < proxy.size(); i++) {
            EXPECT_EQ(vec1[i], proxy[i].value());
        }

        EXPECT_EQ(vec1, proxy.as_array().vec());
        std::vector vec2 = binary::deserialize(proxy);
        EXPECT_EQ(vec1, vec2);
    }
}

TEST(Binary, StringArray) {
    static_assert(check_sections<std::vector<std::string>, std::string, char>);

    std::vector<std::string> sv = {"1", "22", "333", "444"};
    auto [buffer, proxy] = binary::serialize(sv);
    EXPECT_EQ(sv.size(), proxy.size());

    for(auto i = 0; i < sv.size(); i++) {
        EXPECT_EQ(sv[i], proxy[i].as_string());
    }

    std::vector sv2 = binary::deserialize(proxy);
    EXPECT_EQ(sv, sv2);
}

struct Point {
    uint32_t x;
    uint32_t y;
};

TEST(Binary, Struct) {
    {
        static_assert(binary::is_directly_binarizable_v<Point>);
        static_assert(std::same_as<binary::binarify_t<Point>, Point>);
        static_assert(check_sections<Point>);

        Point p = {1, 2};
        auto [buffer, proxy] = binary::serialize(p);
        EXPECT_EQ(proxy->x, 1);
        EXPECT_EQ(proxy->y, 2);
        EXPECT_EQ(p, proxy.value());
        auto p2 = binary::deserialize(proxy);
        EXPECT_EQ(p, p2);
    }

    struct Foo {
        uint32_t age;
        std::string name;
        std::vector<int> scores;
    };

    {
        static_assert(!binary::is_directly_binarizable_v<Foo>);
        static_assert(check_sections<Foo, char, int>);

        Foo foo = {
            0,
            "123",
            {1, 2, 3},
        };

        auto [buffer, proxy] = binary::serialize(foo);
        EXPECT_EQ(proxy.get<"age">().value(), 0);
        EXPECT_EQ(proxy.get<"name">().as_string(), "123");
        EXPECT_EQ(proxy.get<"scores">().as_array().vec(), std::vector{1, 2, 3});
        auto foo2 = binary::deserialize(proxy);
        EXPECT_EQ(foo, foo2);
    };

    struct Points {
        std::vector<Point> points;
    };

    {
        static_assert(!binary::is_directly_binarizable_v<Points>);
        static_assert(check_sections<Points, Point>);

        Points points{
            {
             Point{1, 2},
             Point{3, 4},
             },
        };
        auto [buffer, proxy] = binary::serialize(points);
        auto points2 = proxy.get<"points">();
        EXPECT_EQ(points2[0].value(), Point{1, 2});
        EXPECT_EQ(points2[1].value(), Point{3, 4});
        auto points3 = binary::deserialize(proxy);
        EXPECT_EQ(points, points3);
    }
}

struct Node {
    int value;
    std::vector<Node> nodes;
};

TEST(Binary, Recursively) {
    Node node = {
        1,
        {{3},
          {4},
          {
             5,
             {
                 {3},
                 {4},
                 {5},
             },
         }},
    };

    static_assert(!binary::is_directly_binarizable_v<Node>);
    static_assert(check_sections<Node, Node>);

    auto [buffer, proxy] = binary::serialize(node);
    auto node2 = binary::deserialize(proxy);
    EXPECT_EQ(node, node2);
}

}  // namespace

}  // namespace clice::testing

