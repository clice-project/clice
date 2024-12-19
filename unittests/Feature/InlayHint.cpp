#include <gtest/gtest.h>
#include <Feature/InlayHint.h>

#include "../Test.h"

namespace clice {

namespace {

TEST(InlayHint, AutoDecl) {
    const char* main = R"cpp(
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;

    auto res = feature::inlayHints({}, info, nullptr);

    // dbg(res);

    txs.equal(res.size(), 0)
        //
        ;
}

}  // namespace
}  // namespace clice
