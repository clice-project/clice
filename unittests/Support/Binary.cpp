#include "Test/CTest.h"
#include "Support/Binary.h"

namespace clice::testing {

namespace {

TEST(Binary, Binarify) {
    static_assert(binary::impl::is_directly_binarizable_v<int>);
    static_assert(std::same_as<binary::impl::binarify_t<int>, int>);

    struct Point {
        uint32_t x;
        uint32_t y;
    };

    static_assert(binary::impl::is_directly_binarizable_v<Point>);
    static_assert(std::same_as<binary::impl::binarify_t<Point>, Point>);

    struct Person {
        std::string x;
        uint32_t age;
    };

    static_assert(!binary::impl::is_directly_binarizable_v<Person>);
    static_assert(
        std::same_as<binary::impl::binarify_t<Person>, std::tuple<binary::impl::string, uint32_t>>);

    struct Foo {
        std::vector<int> scores;
    };

    static_assert(!binary::impl::is_directly_binarizable_v<Foo>);
    static_assert(
        std::same_as<binary::impl::binarify_t<Foo>, std::tuple<binary::impl::array<int>>>);

    struct Bar {
        Foo foo;
    };

    static_assert(!binary::impl::is_directly_binarizable_v<Bar>);
    static_assert(std::same_as<binary::impl::binarify_t<Bar>,
                               std::tuple<std::tuple<binary::impl::array<int>>>>);
}

struct Point {
    uint32_t x;
    uint32_t y;
};

TEST(Binary, Simple) {
    using namespace clice::binary;
    auto proxy = binary::binarify(Point{1, 2}).first;

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

    auto proxy = binary::binarify(points).first;

    auto points2 = proxy.get<"points">();

    EXPECT_EQ(points2[0].value(), Point{1, 2});
    EXPECT_EQ(points2[1].value(), Point{3, 4});

    std::free(const_cast<void*>(proxy.base));
}

}  // namespace
}  // namespace clice::testing

