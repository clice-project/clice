#include "Test.h"

namespace clice {

namespace {

struct Color : support::Enum<Color> {
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
}

struct Mask : support::Enum<Mask, true, uint32_t> {
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
}

}  // namespace

}  // namespace clice

