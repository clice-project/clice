#include <gtest/gtest.h>
#include <Index/Pack.h>

namespace {

using namespace clice;

TEST(clice, pack) {
    CSIF csif;
    csif.version = "0.0.1";
    auto data = pack(csif);

    auto result = unpack(data.get());
    EXPECT_EQ(csif.version, result.version);

    std::vector<int> x;
    x.emplace_back(1);
}

}  // namespace
