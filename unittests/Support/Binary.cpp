#include "Test/CTest.h"
#include "Support/Binary.h"

namespace clice::testing {

namespace {

TEST(Binary, Binarify) {
    static_assert(binary::is_directly_binarizable_v<int>);
    static_assert(std::same_as<binary::binarify_t<int>, int>);

    struct Point {
        uint32_t x;
        uint32_t y;
    };

    static_assert(binary::is_directly_binarizable_v<Point>);
    static_assert(std::same_as<binary::binarify_t<Point>, Point>);

    struct Person {
        std::string x;
        uint32_t age;
    };

    static_assert(!binary::is_directly_binarizable_v<Person>);
    static_assert(std::same_as<binary::binarify_t<Person>, std::tuple<binary::string, uint32_t>>);

    struct Foo {
        std::vector<int> scores;
    };

    static_assert(!binary::is_directly_binarizable_v<Foo>);
    static_assert(std::same_as<binary::binarify_t<Foo>, std::tuple<binary::array<int>>>);

    struct Bar {
        Foo foo;
    };

    static_assert(!binary::is_directly_binarizable_v<Bar>);
    static_assert(
        std::same_as<binary::binarify_t<Bar>, std::tuple<std::tuple<binary::array<int>>>>);
}

struct Point {
    uint32_t x;
    uint32_t y;
};

TEST(Binary, Simple) {
    using namespace clice::binary;
    auto proxy = binary::serialize(Point{1, 2}).first;

    EXPECT_EQ(proxy.value().x, 1);
    EXPECT_EQ(proxy.value().y, 2);

    std::free(const_cast<void*>(proxy.base));
}

struct Points {
    std::vector<Point> points;
};

TEST(Binary, Nested) {
    Points points{
        {Point{1, 2}, Point{3, 4}}
    };

    auto proxy = binary::serialize(points).first;

    auto points2 = proxy.get<"points">();

    EXPECT_EQ(points2[0].value(), Point{1, 2});
    EXPECT_EQ(points2[1].value(), Point{3, 4});

    std::free(const_cast<void*>(proxy.base));
}

struct Node {
    int value;
    std::vector<Node> nodes;
};

TEST(Binary, Recursively) {
    Node node = {
        1,
        {
          {3},
          {4},
          {
                5,
                {
                    {3},
                    {4},
                    {5},
                },
            }, },
    };

    auto proxy = binary::serialize(node).first;
    auto node2 = binary::deserialize(proxy);
    EXPECT_EQ(node, node2);
    std::free(const_cast<void*>(proxy.base));
}

}  // namespace
}  // namespace clice::testing

