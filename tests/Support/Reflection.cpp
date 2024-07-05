#include <Support/Reflection.h>

namespace {

struct Point {
    int x;
    int y;
};

static_assert(clice::impl::member_count<Point>() == 2);
}  // namespace
