#include <gtest/gtest.h>
#include <Feature/DocumentSymbol.h>

#include "../Test.h"
#include "Support/Enum.h"

namespace clice {

namespace {

void dbg(const proto::DocumentSymbolResult& result, size_t ident = 0) {
    for(auto& item: result) {
        for(size_t i = 0; i < ident; ++i)
            llvm::outs() << ' ';
        llvm::outs() << std::format("name: {}, kind:{}, detail:{}, deprecated:{}, children_num:{}",
                                    item.name,
                                    item.kind.name(),
                                    item.detail,
                                    item.deprecated,
                                    item.children.size())
                     << '\n';

        dbg(item.children, ident + 2);
    }
}

TEST(DocumentSymbol, NamespaceAndStruct) {
    const char* main = R"cpp(
namespace _1 {
    namespace _2 {
    
    }
}

namespace _1 {
    namespace _2 {
        namespace _3 {
        }
    }
}

namespace {}

namespace _1::_2{
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    dbg(res);
}

}  // namespace

}  // namespace clice
