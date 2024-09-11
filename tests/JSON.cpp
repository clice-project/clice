#include <gtest/gtest.h>
#include <Support/JSON.h>

namespace {

using namespace clice;

TEST(JSON, Point) {
    json::Object object;
    object["x"] = 1;
    object["y"] = 2;

    struct Point {
        int x;
        int y;
    };

    auto point = clice::json::deserialize<Point>(object);
    ASSERT_EQ(point.x, 1);
    ASSERT_EQ(point.y, 2);

    auto result = clice::json::serialize(point);
    ASSERT_EQ(result, json::Value(std::move(object)));
}

}  // namespace
