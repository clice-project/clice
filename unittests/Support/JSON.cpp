#include <gtest/gtest.h>
#include <Support/Struct.h>

namespace clice {

namespace {

TEST(Support, JSON) {
    json::Object object;
    object["x"] = 1;
    object["y"] = 2;

    struct Point {
        int x;
        int y;
    };

    auto point = clice::json::deserialize<Point>(std::move(object));
    ASSERT_EQ(point.x, 1);
    ASSERT_EQ(point.y, 2);

    // auto result = clice::json::serialize(point);
    // ASSERT_EQ(result, json::Value(std::move(object)));
}

}  // namespace

struct ValueRef {
    int index;
};

template <>
struct json::Serde<ValueRef> {
    constexpr inline static bool state = true;

    std::vector<int>& decoder;

    json::Value serialize(const ValueRef& ref) {
        return json::Value(decoder[ref.index]);
    }
};

TEST(Support, StatefulSerde) {
    std::vector<ValueRef> refs;
    refs.emplace_back(4);
    refs.emplace_back(3);
    refs.emplace_back(2);
    refs.emplace_back(1);
    refs.emplace_back(0);

    std::vector<int> decoder = {1, 2, 3, 4, 5};
    json::Serde<ValueRef> serde{decoder};
    auto result = json::serialize(refs, serde);
    ASSERT_EQ(result, json::Value(json::Array{5, 4, 3, 2, 1}));
}

}  // namespace clice
