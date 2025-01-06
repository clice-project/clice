#include <gtest/gtest.h>
#include <Feature/InlayHint.h>

#include "../Test.h"

namespace clice {

namespace {

void dbg(const std::vector<proto::InlayHint>& hints) {
    for(auto& hint: hints) {
        llvm::outs() << std::format("kind:{}, position:{}, value_size:{},",
                                    hint.kind.name(),
                                    json::serialize(hint.position),
                                    hint.lable.size());

        for(auto& lable: hint.lable) {
            llvm::outs() << std::format(" value:{}, link position:{}",
                                        lable.value,
                                        json::serialize(lable.Location))
                         << '\n';
        }
    }
}

TEST(InlayHint, AutoDecl) {
    const char* main = R"cpp(
auto$(1) x = 1;

void f() {
    const auto&$(2) x_ref = x;

    if (auto$(3) z = x + 1) {}

    for(auto$(4) i = 0; i<10; ++i) {}
}

template<typename T>
void t() {
    auto z = T{};
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

    txs.equal(res.size(), 4)
        //
        ;
}

}  // namespace
}  // namespace clice
