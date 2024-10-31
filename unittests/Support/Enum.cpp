#include "../Test.h"
#include <Support/Enum.h>

namespace {
using namespace clice;

enum class ColorKinds {
    Red,
    Green,
    Blue,
    Yellow,
};

struct ColorKind : enum_type<ColorKinds> {
    using enum ColorKinds;
    using enum_type::enum_type;
};

TEST(Support, normal_enum) {
    ColorKind color = ColorKind::Red;

    std::string string;
    llvm::raw_string_ostream stream(string);
    stream << color;
    stream.flush();
    EXPECT_EQ(string, "[normal enum, value: 0, name: Red]");

    EXPECT_EQ(underlying_value(color), 0);
}

enum class MaskKinds {
    A = 0,
    B,
    C,
    D,
};

struct MaskKind : enum_type<MaskKinds, true> {
    using enum MaskKinds;
    using enum_type::enum_type;
};

TEST(Support, mask_enum) {
    MaskKind mask = MaskKind::A;
    mask.set(MaskKind::B);
    EXPECT_TRUE(mask.is(MaskKind::A));
    EXPECT_TRUE(mask.is(MaskKind::B));
    EXPECT_FALSE(mask.is(MaskKind::C));
    EXPECT_FALSE(mask.is(MaskKind::D));
    EXPECT_EQ(underlying_value(mask), 3);

    std::string string;
    llvm::raw_string_ostream stream(string);
    stream << mask;
    stream.flush();
    EXPECT_EQ(string, "[mask enum, value: 3, masks: A | B]");

    MaskKind mask2 = MaskKind::C;
    EXPECT_FALSE(mask == mask2);
}

}  // namespace

