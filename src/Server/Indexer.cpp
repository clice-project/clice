#include "Index/Index.h"
#include "Server/Indexer.h"

namespace clice {

async::Task<> Indexer::index(ASTInfo& AST) {
    auto result = co_await async::submit([&] { return index::memory::index(AST); });

    auto& [graph, indices] = result;
    for(auto& [fid, index]: indices) {}
}

}  // namespace clice
