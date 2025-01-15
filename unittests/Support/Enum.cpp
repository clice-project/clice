#include "Test/Test.h"
#include "Support/Enum.h"
#include "Support/JSON.h"

namespace clice::testing {

namespace {

struct Color : refl::Enum<Color> {
    enum Kind : uint8_t {
        Red,
        Green,
        Blue,
        Yellow,
        Invalid,
    };

    using Enum::Enum;

    constexpr inline static auto InvalidEnum = Invalid;
};

TEST(Support, NormalEnum) {
    constexpr Color invalid = {};
    static_assert(!invalid);
    static_assert(invalid.value() == Color::Invalid);

    constexpr Color red = Color::Red;
    constexpr Color green = Color::Green;
    constexpr Color blue = Color::Blue;
    constexpr Color yellow = Color::Yellow;

    static_assert(red.name() == "Red" && red.value() == 0);
    static_assert(green.name() == "Green" && green.value() == 1);
    static_assert(blue.name() == "Blue" && blue.value() == 2);
    static_assert(yellow.name() == "Yellow" && yellow.value() == 3);

    static_assert(red != green && red != blue && red != yellow);

    constexpr Color red2 = Color(0);
    static_assert(red == red2);

    EXPECT_EQ(json::serialize(red), json::Value(0));
    EXPECT_EQ(json::serialize(green), json::Value(1));
    EXPECT_EQ(json::serialize(blue), json::Value(2));
    EXPECT_EQ(json::serialize(yellow), json::Value(3));

    EXPECT_EQ(json::deserialize<Color>(json::Value(0)), red);
    EXPECT_EQ(json::deserialize<Color>(json::Value(1)), green);
    EXPECT_EQ(json::deserialize<Color>(json::Value(2)), blue);
    EXPECT_EQ(json::deserialize<Color>(json::Value(3)), yellow);
}

struct Mask : refl::Enum<Mask, true, uint32_t> {
    enum Kind {
        A = 0,
        B,
        C,
        D,
    };

    using Enum::Enum;
};

TEST(Support, MaskEnum) {
    constexpr Mask invalid = {};
    static_assert(!invalid);
    static_assert(invalid.value() == 0);

    constexpr Mask mask = Mask::A;
    constexpr Mask mask2 = Mask::B;
    constexpr Mask mask3 = Mask::C;
    constexpr Mask mask4 = Mask::D;

    static_assert(mask.name() == "A" && mask.value() == 1);
    static_assert(mask2.name() == "B" && mask2.value() == 2);
    static_assert(mask3.name() == "C" && mask3.value() == 4);
    static_assert(mask4.name() == "D" && mask4.value() == 8);

    static_assert(mask != mask2 && mask != mask3 && mask != mask4);

    constexpr Mask mask5 = Mask(Mask::A, Mask::B, Mask::C, Mask::D);
    static_assert(mask5.name() == "A | B | C | D" && mask5.value() == 15);

    Mask mask6 = Mask::A;
    mask6 |= Mask::B;
    EXPECT_EQ(mask6.name(), "A | B");
    EXPECT_TRUE(bool(mask6 & Mask::A));
    EXPECT_TRUE(bool(mask6 & Mask::B));

    mask6 |= Mask::C;
    EXPECT_EQ(mask6.name(), "A | B | C");
    EXPECT_TRUE(bool(mask6 & Mask::A));
    EXPECT_TRUE(bool(mask6 & Mask::B));
    EXPECT_TRUE(bool(mask6 & Mask::C));

    EXPECT_EQ(json::serialize(mask), json::Value(1));
    EXPECT_EQ(json::serialize(mask2), json::Value(2));
    EXPECT_EQ(json::serialize(mask3), json::Value(4));
    EXPECT_EQ(json::serialize(mask4), json::Value(8));

    EXPECT_EQ(json::deserialize<Mask>(json::Value(1)), mask);
    EXPECT_EQ(json::deserialize<Mask>(json::Value(2)), mask2);
    EXPECT_EQ(json::deserialize<Mask>(json::Value(4)), mask3);
    EXPECT_EQ(json::deserialize<Mask>(json::Value(8)), mask4);
}

struct StringEnum : refl::Enum<StringEnum, false, std::string_view> {
    using Enum::Enum;

    constexpr inline static std::string_view A = "A";
    constexpr inline static std::string_view B = "B";
    constexpr inline static std::string_view C = "C";
    constexpr inline static std::string_view D = "D";

    constexpr inline static std::array All = {A, B, C, D};
};

TEST(Support, StringEnum) {
    constexpr StringEnum a = StringEnum::A;
    constexpr StringEnum b = StringEnum::B;
    constexpr StringEnum c = StringEnum::C;
    constexpr StringEnum d = StringEnum::D;

    static_assert(a.value() == "A");
    static_assert(b.value() == "B");
    static_assert(c.value() == "C");
    static_assert(d.value() == "D");

    static_assert(a != b && a != c && a != d);

    EXPECT_EQ(json::serialize(a), json::Value("A"));
    EXPECT_EQ(json::serialize(b), json::Value("B"));
    EXPECT_EQ(json::serialize(c), json::Value("C"));
    EXPECT_EQ(json::serialize(d), json::Value("D"));

    EXPECT_EQ(json::deserialize<StringEnum>(json::Value("A")), a);
    EXPECT_EQ(json::deserialize<StringEnum>(json::Value("B")), b);
    EXPECT_EQ(json::deserialize<StringEnum>(json::Value("C")), c);
    EXPECT_EQ(json::deserialize<StringEnum>(json::Value("D")), d);
}

}  // namespace

}  // namespace clice

