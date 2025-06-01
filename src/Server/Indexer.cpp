#include "Compiler/AST.h"
#include "Index/Index.h"
#include "Server/Indexer.h"

namespace clice {

async::Task<> Indexer::index(ASTInfo& AST) {
    auto [tu_index, header_indices] =
        co_await async::submit([&] { return index::memory::index(AST); });

    auto tu_id = getPath(tu_index->path);

    llvm::DenseSet<PathID> visited_headers;

    for(auto& [fid, index]: header_indices) {
        auto id = getPath(index->path);
        auto it = dynamic_header_indices.find(id);
        auto include = tu_index->graph.getInclude(fid);

        if(it != dynamic_header_indices.end()) {
            auto& indices = it->second;

            if(!visited_headers.contains(id)) {
                indices->unmergeds[tu_id].clear();
                visited_headers.insert(id);
            }

            indices->unmergeds[tu_id].emplace_back(include, std::move(index));
        } else {
            auto [it, _] = dynamic_header_indices.try_emplace(id, new HeaderIndices());
            it->second->unmergeds[tu_id].emplace_back(include, std::move(index));
            visited_headers.insert(id);
        }
    }

    dynamic_tu_indices[tu_id] = std::move(tu_index);
}

}  // namespace clice
