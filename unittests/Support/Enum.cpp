#include "../Test.h"
#include <Support/Enum.h>

namespace {

using namespace clice;

struct Color : support::Enum<Color> {
    using Enum::Enum;

    enum Kind : uint8_t {
        Red,
        Green,
        Blue,
        Yellow,
    };
};

TEST(Support, normal_enum) {
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

struct Mask : support::Enum<Mask, true> {
    using Enum::Enum;

    enum Kind {
        A = 0,
        B,
        C,
        D,
    };
};

TEST(Support, mask_enum) {
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
}



}  // namespace

