#include "Compiler/CompilationUnit.h"
#include "Compiler/Compilation.h"
#include "Index/Index.h"
#include "Server/Indexer.h"
#include "Support/Logger.h"

namespace clice {

async::Task<> Indexer::index(CompilationUnit& unit) {
    auto [tu_index, header_indices] =
        co_await async::submit([&] { return index::memory::index(unit); });

    auto tu_id = getPath(tu_index->path);

    llvm::DenseSet<PathID> visited_headers;

    for(auto& [fid, index]: header_indices) {
        auto id = getPath(index->path);
        auto it = dynamic_header_indices.find(id);

        /// FIXME: If the value less than 0, it represents the file is from
        /// PCH or module. How to handle such file?
        if(fid < clang::FileID::getSentinel()) {
            continue;
        }

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

async::Task<> Indexer::index(llvm::StringRef file) {
    CompilationParams params;
    params.command = database.getCommand(file);

    auto AST = co_await async::submit([&] { return compile(params); });

    if(!AST) {
        log::info("Fail to index background file {}", file);
        co_return;
    }

    co_await index(*AST);
}

}  // namespace clice
