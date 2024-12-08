#include "Test.h"

namespace clice {

namespace {
struct ValueRef {
    std::size_t index;
};
}  // namespace

template <>
struct json::Serde<ValueRef> {
    constexpr inline static bool stateful = true;

    std::vector<int>& encoder;
    std::vector<int>& decoder;

    json::Value serialize(const ValueRef& ref) {
        return json::Value(decoder[ref.index]);
    }

    ValueRef deserialize(const json::Value& value) {
        encoder.emplace_back(*value.getAsInteger());
        return ValueRef{encoder.size() - 1};
    }
};

namespace {

TEST(Support, JSON) {
    struct Point {
        int x;
        int y;
    };

    json::Value object = json::Object{
        {"x", 1},
        {"y", 2},
    };

    auto point = json::deserialize<Point>(std::move(object));
    ASSERT_EQ(point.x, 1);
    ASSERT_EQ(point.y, 2);

    auto result = json::serialize(point);
    ASSERT_EQ(result, object);
}

TEST(Support, StatefulSerde) {
    struct Refs {
        std::vector<ValueRef> data;
    };

    static_assert(json::stateful_serde<Refs>);

    Refs refs;
    refs.data.emplace_back(4);
    refs.data.emplace_back(3);
    refs.data.emplace_back(2);
    refs.data.emplace_back(1);
    refs.data.emplace_back(0);

    std::vector<int> encoder;
    std::vector<int> decoder = {1, 2, 3, 4, 5};
    json::Serde<ValueRef> serde{encoder, decoder};

    auto result = json::serialize(refs, serde);
    ASSERT_EQ(result,
              json::Value(json::Object{
                  {"data", {5, 4, 3, 2, 1}}
    }));

    auto refs2 = json::deserialize<Refs>(result, serde);
    ASSERT_EQ(refs2.data.size(), 5);
    for(std::size_t i = 0; i < refs2.data.size(); ++i) {
        ASSERT_EQ(refs2.data[i].index, i);
    }
}

}  // namespace

}  // namespace clice
