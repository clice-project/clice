#include "Server/Indexer.h"
#include "Server/IncludeGraph.h"
#include "Support/Logger.h"

namespace clice {

Indexer::Indexer(CompilationDatabase& database, const config::IndexOptions& options) :
    IncludeGraph(options), database(database), options(options) {}

void Indexer::add(std::string file) {
    /// If the file is already indexed, cancel and start a new indexing task.
    if(auto it = tasks.find(file); it != tasks.end()) [[unlikely]] {
        auto& task = it->second;
        assert(!task.done() && "if task is done, it should be removed from the map");
        task.cancel();
        task.dispose();
        task = index(file);
        task.schedule();
        return;
    }

    /// If the size of the tasks list is less than the concurrency, add the file
    /// to the tasks list and start a new indexing task.
    if(tasks.size() != concurrency) {
        auto task = index(file);
        task.schedule();
        tasks.try_emplace(file, std::move(task));
        pending.erase(file);
        return;
    }

    /// Finally, all tasks are running, add the file to the pending list.
    pending.insert(file);
}

void Indexer::remove(std::string file) {
    if(pending.contains(file)) {
        pending.erase(file);
        return;
    }

    if(auto it = tasks.find(file); it != tasks.end()) {
        it->second.cancel();
        tasks.erase(it);

        return;
    }
}

void Indexer::indexAll() {
    for(auto& entry: database) {
        add(entry.first().str());
    }
}

void Indexer::save() {
    auto json = IncludeGraph::dump();
    auto result = fs::write(path::join(options.dir, "index.json"), std::format("{}", json));
    if(result) {
        log::info("Successfully saved index to disk");
    } else {
        log::warn("Failed to save index to disk: {}", result.error());
    }
}

void Indexer::load() {
    auto path = path::join(options.dir, "index.json");
    auto file = fs::read(path);
    if(!file) {
        log::warn("Failed to open index file: {}", file.error());
        return;
    }

    if(auto result = json::parse(file.value())) {
        IncludeGraph::load(*result);
        log::info("Successfully loaded index from disk");
    } else {
        log::warn("Failed to parse index file: {}", result.takeError());
    }
}

async::Task<> Indexer::index(std::string file) {
    assert(!pending.contains(file) && "file should not be in the pending list");
    assert(tasks.contains(file) && "file should not be in the tasks list");

    auto task = IncludeGraph::index(file, database);
    co_await task;

    log::info("index process: [running: {}, pending :{}], finish {}",
              tasks.size() - 1,
              pending.size(),
              file);

    auto it = tasks.find(file);
    assert(it != tasks.end() && "file should be in the tasks list");
    /// We cannot directly erase the task from the map, this will call
    /// its destructor and destroy the coroutine. But we are still
    /// executing the coroutine. So dispose it.
    it->second.dispose();
    tasks.erase(it);

    if(pending.empty()) {
        co_return;
    }

    /// Create a new task for the next file and schedule it.
    file = pending.begin()->first();
    auto next = index(file);
    next.schedule();

    /// Remove the file from the pending list and add it to the tasks list.
    pending.erase(pending.begin());
    tasks.try_emplace(file, std::move(next));
}

Header* Indexer::getHeader(llvm::StringRef file) const {
    auto it = headers.find(file);
    if(it == headers.end()) {
        return nullptr;
    }
    return it->second;
}

TranslationUnit* Indexer::getTranslationUnit(llvm::StringRef file) const {
    auto it = tus.find(file);
    if(it == tus.end()) {
        return nullptr;
    }
    return it->second;
}

std::optional<proto::HeaderContext> Indexer::currentContext(llvm::StringRef file) const {
    auto header = getHeader(file);
    if(!header || !header->active.valid()) {
        return std::nullopt;
    }

    auto& active = header->active;
    return proto::HeaderContext{
        .file = active.tu->srcPath,
        .version = active.tu->version,
        .include = active.context.include,
    };
}

bool Indexer::switchContext(llvm::StringRef headerFile, proto::HeaderContext context) {
    Header* header = getHeader(headerFile);
    if(!header) {
        return false;
    }

    TranslationUnit* tu = getTranslationUnit(context.file);
    if(!tu || tu->version != context.version) {
        return false;
    }

    auto index = header->getIndex(tu, context.include);
    if(!index) {
        return false;
    }

    header->active = HeaderContext{
        .tu = tu,
        .context = {.index = *index, .include = context.include},
    };

    return true;
}

std::vector<proto::IncludeLocation> Indexer::resolveContext(proto::HeaderContext context) const {
    auto tu = getTranslationUnit(context.file);
    if(!tu || tu->version != context.version) {
        return {};
    }

    auto include = context.include;
    auto& locations = tu->locations;
    if(locations.size() <= include) [[unlikely]] {
        /// FIXME: This should never occur, we should log.
        return {};
    }

    std::vector<proto::IncludeLocation> result;
    while(include != -1) {
        auto& location = locations[include];
        result.emplace_back(proto::IncludeLocation{
            .line = location.line - 1,
            .file = pathPool[location.file],
        });
        include = location.include;
    }
    return result;
}

std::vector<proto::HeaderContextGroup> Indexer::allContexts(llvm::StringRef headerFile,
                                                            uint32_t limit,
                                                            llvm::StringRef contextFile) const {
    auto header = getHeader(headerFile);
    if(!header) {
        return {};
    }

    llvm::DenseMap<uint32_t, std::vector<proto::HeaderContext>> groups;

    if(auto tu = getTranslationUnit(contextFile)) {
        for(auto context: header->contexts[tu]) {
            groups[context.index].emplace_back(proto::HeaderContext{
                .file = tu->srcPath,
                .version = tu->version,
                .include = context.include,
            });
        }
    } else {
        for(auto&& [tu, contexts]: header->contexts) {
            for(auto& context: contexts) {
                auto& group = groups[context.index];
                if(group.size() >= 10) {
                    continue;
                }

                group.emplace_back(proto::HeaderContext{
                    .file = tu->srcPath,
                    .version = tu->version,
                    .include = context.include,
                });
            }
        }
    }

    std::vector<proto::HeaderContextGroup> result;
    for(auto& [index, group]: groups) {
        result.emplace_back(header->indices[index].path, std::move(group));
    }
    return result;
}

std::vector<std::string> Indexer::indices(TranslationUnit* tu) {
    std::vector<std::string> indices;
    if(tu) {
        indices.emplace_back(tu->indexPath);
        for(auto& header: tu->headers) {
            for(auto& context: header->contexts[tu]) {
                indices.emplace_back(header->indices[context.index].path);
            }
        }
    } else {
        for(auto& [_, tu]: tus) {
            indices.emplace_back(tu->indexPath);
        }

        for(auto& [_, header]: headers) {
            for(auto& index: header->indices) {
                indices.emplace_back(index.path);
            }
        }
    }
    return indices;
}

async::Task<std::vector<Indexer::SymbolID>>
    Indexer::resolve(const proto::TextDocumentPositionParams& params) {
    std::vector<SymbolID> ids;
    auto path = SourceConverter::toPath(params.textDocument.uri);

    std::uint32_t offset = 0;
    {
        auto content = co_await async::fs::read(path);
        if(!content) {
            co_return ids;
        }
        offset = SC.toOffset(*content, params.position);
    }

    std::vector<std::string> indices;
    if(auto iter = tus.find(path); iter != tus.end()) {
        indices.emplace_back(iter->second->indexPath);
    } else if(auto iter = headers.find(path); iter != headers.end()) {
        /// FIXME: What is the expected result of resolving a symbol that may refers different
        /// declaration in different header contexts? Currently, we just return the first one.
        for(auto& index: iter->second->indices) {
            indices.emplace_back(index.path);
        }
    }

    co_await async::gather(indices, [&](const std::string& path) -> async::Task<bool> {
        auto binary = co_await async::fs::read(path + ".sidx");
        if(!binary) {
            co_return true;
        }

        llvm::SmallVector<index::SymbolIndex::Symbol> symbols;
        co_await async::submit([&] {
            index::SymbolIndex index(binary->data(), binary->size(), false);
            index.locateSymbols(offset, symbols);
        });

        if(symbols.empty()) {
            co_return true;
        }

        /// If we found the symbol, we just return it. And break other tasks.
        for(auto& symbol: symbols) {
            ids.emplace_back(symbol.id(), symbol.name().str());
        }
        co_return false;
    });

    co_return ids;
}

async::Task<> Indexer::lookup(llvm::ArrayRef<SymbolID> targets,
                              llvm::ArrayRef<std::string> files,
                              LookupCallback callback) {
    co_await async::gather(files, [&](const std::string& indexPath) -> async::Task<bool> {
        auto binary = co_await async::fs::read(indexPath + ".sidx");
        if(!binary) {
            co_return false;
        }

        llvm::SmallVector<index::SymbolIndex::Symbol> symbols;
        index::SymbolIndex index(binary->data(), binary->size(), false);
        co_await async::submit([&] {
            for(auto& target: targets) {
                if(auto symbol = index.locateSymbol(target.hash, target.name)) {
                    symbols.emplace_back(*symbol);
                }
            }
        });

        if(symbols.empty()) {
            co_return true;
        }

        auto srcPath = index.path().str();
        auto content = co_await async::fs::read(srcPath);
        if(!content) {
            co_return true;
        }

        for(auto& symbol: symbols) {
            if(!callback(srcPath, *content, symbol)) {
                co_return false;
            }
        }

        co_return true;
    });
}

async::Task<proto::ReferenceResult> Indexer::lookup(const proto::ReferenceParams& params,
                                                    RelationKind kind) {
    auto ids = co_await resolve(params);
    /// FIXME: If the size of ids equal to one and it is not an external symbol, we should
    /// just search the symbol in the current translation unit.

    proto::ReferenceResult result;

    std::vector<std::string> files = indices();
    co_await lookup(ids,
                    files,
                    [&](llvm::StringRef path,
                        llvm::StringRef content,
                        const index::SymbolIndex::Symbol& symbol) {
                        /// FIXME: We may should collect and sort the ranges in one file.
                        /// So that we can cut off the context to speed up the process.
                        for(auto relation: symbol.relations()) {
                            if(relation.kind() & kind) {
                                result.emplace_back(proto::Location{
                                    .uri = SourceConverter::toURI(path),
                                    .range = SC.toRange(*relation.range(), content),
                                });
                            }
                        }
                        return true;
                    });

    co_return result;
}

async::Task<proto::HierarchyPrepareResult>
    Indexer::prepareHierarchy(const proto::HierarchyPrepareParams& params) {
    auto ids = co_await resolve(params);

    proto::HierarchyPrepareResult result;
    std::vector<std::string> files = indices();
    co_return result;
}

async::Task<std::optional<index::FeatureIndex>>
    Indexer::getFeatureIndex(std::string& buffer, llvm::StringRef file) const {
    std::string path;
    if(auto tu = tus.find(file); tu != tus.end()) {
        path = tu->second->indexPath;
    }

    if(auto header = headers.find(file); header != headers.end()) {
        for(auto&& index: header->second->indices) {
            path = index.path;
            break;
        }
    }

    auto content = co_await async::fs::read(path + ".fidx");
    if(!content) {
        co_return std::nullopt;
    }

    buffer = std::move(*content);

    co_return index::FeatureIndex(buffer.data(), buffer.size(), false);
}

async::Task<std::vector<feature::SemanticToken>>
    Indexer::semanticTokens(llvm::StringRef file) const {
    std::vector<feature::SemanticToken> result;

    std::string buffer;
    auto index = co_await getFeatureIndex(buffer, file);
    if(!index) {
        co_return result;
    }

    co_return index->semanticTokens();
}

async::Task<std::vector<feature::FoldingRange>> Indexer::foldingRanges(llvm::StringRef file) const {
    std::vector<feature::FoldingRange> result;

    std::string buffer;
    auto index = co_await getFeatureIndex(buffer, file);
    if(!index) {
        co_return result;
    }

    co_return index->foldingRanges();
}

}  // namespace clice
