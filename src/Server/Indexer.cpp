#include <random>

#include "Compiler/Compilation.h"
#include "Index/SymbolIndex.h"
#include "Index/FeatureIndex.h"
#include "Support/Logger.h"
#include "Server/Indexer.h"
#include "Support/Assert.h"
#include "Support/Compare.h"

#include "llvm/ADT/StringRef.h"

namespace clice {

Indexer::~Indexer() {
    for(auto& [_, header]: headers) {
        delete header;
    }

    for(auto& [_, tu]: tus) {
        delete tu;
    }
}

async::Task<TranslationUnit*> Indexer::check(this Self& self, llvm::StringRef file) {
    auto iter = self.tus.find(file);

    /// If no translation unit found, we need to create a new one.
    if(iter == self.tus.end()) {
        auto tu = new TranslationUnit;
        tu->srcPath = file.str();
        self.tus.try_emplace(file, tu);
        co_return tu;
    }

    auto tu = iter->second;

    async::Lock lock(self.locked);
    co_await lock;

    /// Otherwise, we need to check whether the file needs to be updated.
    auto stats = co_await async::fs::stat(tu->srcPath);
    if(stats.has_value() && stats->mtime > tu->mtime) {
        co_return tu;
    }

    for(auto header: tu->headers) {
        auto stats = co_await async::fs::stat(header->srcPath);
        if(stats.has_value() && stats->mtime > tu->mtime) {
            co_return tu;
        }
    }

    /// If no need to update, just return nullptr.
    co_return nullptr;
}

uint32_t Indexer::addIncludeChain(std::vector<IncludeLocation>& locations,
                                  llvm::DenseMap<clang::FileID, uint32_t>& files,
                                  clang::SourceManager& SM,
                                  clang::FileID fid) {
    auto [iter, success] = files.try_emplace(fid, locations.size());
    if(!success) {
        return iter->second;
    }

    auto index = iter->second;
    locations.emplace_back();
    auto entry = SM.getFileEntryRefForID(fid);
    assert(entry && "Invalid file entry");

    {
        auto path = path::real_path(entry->getName());
        auto [iter, success] = pathIndices.try_emplace(path, pathPool.size());
        locations[index].filename = iter->second;
    }

    if(auto presumed = SM.getPresumedLoc(SM.getIncludeLoc(fid), false); presumed.isValid()) {
        locations[index].line = presumed.getLine();
        auto include = addIncludeChain(locations, files, SM, presumed.getFileID());
        locations[index].include = include;
    }

    return index;
}

void Indexer::addContexts(this Self& self,
                          ASTInfo& info,
                          TranslationUnit* tu,
                          llvm::DenseMap<clang::FileID, uint32_t>& files) {
    auto& SM = info.srcMgr();

    std::vector<IncludeLocation> locations;

    for(auto& [fid, directive]: info.directives()) {
        for(auto& include: directive.includes) {
            /// If the include is invalid, it indicates that the file is skipped because of
            /// include guard, or `#pragma once`. Such file cannot provide header context.
            /// So we just skip it.
            if(include.fid.isInvalid()) {
                continue;
            }

            /// Add all include chains.
            self.addIncludeChain(locations, files, SM, include.fid);
        }
    }

    /// Update the translation unit.
    tu->mtime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    tu->locations = std::move(locations);

    /// Update the header context.
    for(auto& [fid, include]: files) {
        if(fid == SM.getMainFileID()) {
            continue;
        }

        auto entry = SM.getFileEntryRefForID(fid);
        assert(entry && "Invalid file entry");
        auto name = path::real_path(entry->getName());

        Header* header = nullptr;
        if(auto iter = self.headers.find(name); iter != self.headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = name;
            self.headers.try_emplace(name, header);
        }

        /// Add new header context.
        auto contexts = header->contexts[tu];
        auto iter = ranges::find_if(contexts, [&](const Context& context) {
            return context.include == include;
        });

        if(iter == contexts.end()) {
            header->contexts[tu].emplace_back(Context{
                .include = include,
            });
        }
    }
}

async::Task<> Indexer::updateIndices(this Self& self,
                                     ASTInfo& info,
                                     TranslationUnit* tu,
                                     llvm::DenseMap<clang::FileID, uint32_t>& files) {
    struct Index {
        llvm::XXH128_hash_t symbolHash = {0, 0};
        std::optional<index::SymbolIndex> symbol;

        llvm::XXH128_hash_t featureHash = {0, 0};
        std::optional<index::FeatureIndex> feature;
    };

    auto indices = co_await async::submit([&info] {
        llvm::DenseMap<clang::FileID, Index> indices;

        auto symbolIndices = index::index(info);
        for(auto& [fid, index]: symbolIndices) {
            indices[fid].symbol.emplace(std::move(index));
        }

        auto featureIndices = index::indexFeature(info);
        for(auto& [fid, index]: featureIndices) {
            indices[fid].feature.emplace(std::move(index));
        }

        for(auto& [fid, index]: indices) {
            if(index.symbol) {
                auto data = llvm::ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(index.symbol->base),
                                                    index.symbol->size);
                index.symbolHash = llvm::xxh3_128bits(data);
            }

            if(index.feature) {
                auto data = llvm::ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(index.feature->base),
                                                    index.feature->size);
                index.featureHash = llvm::xxh3_128bits(data);
            }
        }

        return indices;
    });

    async::Lock lock(self.locked);
    co_await lock;

    auto& SM = info.srcMgr();

    for(auto& [fid, index]: indices) {
        if(fid == SM.getMainFileID()) {
            if(tu->indexPath.empty()) {
                tu->indexPath = self.getIndexPath(tu->srcPath);
            }

            if(index.symbol) {
                co_await async::fs::write(tu->indexPath + ".sidx",
                                          index.symbol->base,
                                          index.symbol->size);
            }

            if(index.feature) {
                co_await async::fs::write(tu->indexPath + ".fidx",
                                          index.feature->base,
                                          index.feature->size);
            }

            continue;
        }

        auto entry = SM.getFileEntryRefForID(fid);
        if(!entry) {
            log::info("Invalid file entry for file id: {}", fid.getHashValue());
        }
        assert(entry && "Invalid file entry");

        auto name = path::real_path(entry->getName());
        assert(self.headers.contains(name) && "Invalid header name");

        auto header = self.headers[name];

        auto include = files[fid];
        auto iter = ranges::find_if(header->contexts[tu], [&](const Context& context) {
            return context.include == include;
        });
        assert(iter != header->contexts[tu].end() && "Invalid include index");

        /// Found whether the we already have the same index. If so, use it directly.
        /// Otherwise, we need to create a new index.

        auto& indices = header->indices;

        bool existed = false;
        for(std::size_t i = 0; i < indices.size(); ++i) {
            auto& element = indices[i];
            if(index.symbolHash == element.symbolHash && index.featureHash == element.featureHash) {
                existed = true;
                iter->index = i;
                break;
            }
        }

        if(existed) {
            continue;
        }

        iter->index = static_cast<uint32_t>(indices.size());
        indices.emplace_back(HeaderIndex{
            .path = self.getIndexPath(name),
            .symbolHash = index.symbolHash,
            .featureHash = index.featureHash,
        });

        if(header->srcPath ==
           "/home/ykiko/C++/llvm-project/build-debug-install/include/llvm/Support/Compiler.h") {
            if(index.symbol) {
                auto json = index.symbol->toJSON();
                llvm::SmallString<128> path;
                llvm::raw_svector_ostream stream(path);
                stream << json;

                co_await async::fs::write(indices.back().path + ".json", path.data(), path.size());
            }
        }
        //
        //    // if(index.feature) {
        //    //     println("{{ hash: {}, index: {} }}",
        //    //             json::serialize(index.featureHash),
        //    //             json::serialize(index.feature->semanticTokens()));
        //    // }
        //}

        if(index.symbol) {
            co_await async::fs::write(indices.back().path + ".sidx",
                                      index.symbol->base,
                                      index.symbol->size);
        }

        if(index.feature) {
            co_await async::fs::write(indices.back().path + ".fidx",
                                      index.feature->base,
                                      index.feature->size);
        }
    }
}

async::Task<> Indexer::index(this Self& self, llvm::StringRef file) {
    auto path = path::real_path(file);
    file = path;

    auto tu = co_await self.check(file);
    if(!tu) {
        log::info("No need to update index for file: {}", file);
        co_return;
    }

    auto command = self.database.getCommand(file);
    if(command.empty()) {
        log::warn("No command found for file: {}", file);
        co_return;
    }

    CompilationParams params;
    params.command = command;

    auto info = co_await async::submit([&params] { return compile(params); });
    if(!info) {
        log::warn("Failed to compile {}: {}", file, info.error());
        co_return;
    }

    llvm::DenseMap<clang::FileID, uint32_t> files;

    /// Otherwise, we need to update all header contexts.
    self.addContexts(*info, tu, files);

    co_await self.updateIndices(*info, tu, files);
}

async::Task<> Indexer::index(llvm::StringRef file, ASTInfo& info) {
    co_return;
}

async::Task<> Indexer::indexAll() {
    auto total = database.size();
    auto count = 0;

    auto each = [&](llvm::StringRef file) -> async::Task<> {
        count += 1;
        log::info("Indexing process: {}/{}, file: {}", count, total, file);
        co_await index(file);
    };

    auto iter = database.begin();
    auto end = database.end();

    std::vector<async::Task<>> tasks;
    /// TODO: Use threads count in the future.
    tasks.resize(20);

    log::info("Start indexing all files");

    while(iter != end ||
          ranges::any_of(tasks, [](auto& task) { return !task.empty() && !task.done(); })) {
        for(auto& task: tasks) {
            if(task.empty() || task.done()) {
                if(iter != end) {
                    task = each(iter->first());
                    async::schedule(task.handle());
                    ++iter;
                }
            }
        }

        co_await async::suspend([&](auto handle) { async::schedule(handle); });
    }
}

std::string Indexer::getIndexPath(llvm::StringRef file) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000);
    return path::join(options.dir, path::filename(file) + "." + llvm::Twine(ms + dis(gen)));
}

json::Value Indexer::dumpToJSON() {
    json::Array headers;
    for(auto& [_, header]: this->headers) {
        json::Array contexts;
        for(auto& [tu, context]: header->contexts) {
            contexts.emplace_back(json::Object{
                {"tu",       tu->srcPath             },
                {"contexts", json::serialize(context)},
            });
        }

        headers.emplace_back(json::Object{
            {"srcPath",  header->srcPath                 },
            {"contexts", std::move(contexts)             },
            {"indices",  json::serialize(header->indices)},
        });
    }

    json::Array tus;
    for(auto& [_, tu]: this->tus) {
        tus.emplace_back(json::Object{
            {"srcPath",   tu->srcPath                   },
            {"indexPath", tu->indexPath                 },
            {"mtime",     tu->mtime.count()             },
            {"locations", json::serialize(tu->locations)},
        });
    }

    return json::Object{
        {"headers", std::move(headers)       },
        {"tus",     std::move(tus)           },
        {"paths",   json::serialize(pathPool)},
    };
}

std::string Indexer::getIndexPath(const HeaderContext& context) {
    if(context.contextFile.empty()) {
        auto tu = tus[context.srcFile];
        return tu->indexPath;
    } else {
        auto include = context.include;
        auto header = headers[context.contextFile];
        auto tu = tus[context.srcFile];
        for(auto& context: header->contexts[tu]) {
            if(context.include == include) {
                return header->indices[context.index].path;
            }
        }
    }

    return "";
}

async::Task<> Indexer::lookup(const HeaderContext& context,
                              uint32_t offset,
                              LookupCallback callback) {
    std::string indexPath = getIndexPath(context);
    if(indexPath.empty()) {
        co_return;
    }

    auto content = co_await async::fs::read(indexPath + ".sidx");
    if(!content) {
        log::warn("Failed to read index file: {}", indexPath + ".sidx");
        co_return;
    }

    llvm::SmallVector<index::SymbolIndex::Symbol, 8> symbols;
    index::SymbolIndex sindex(content->data(), content->size(), false);

    /// TODO: This task may be too slow, we may need to schedule it in thread pool.
    sindex.locateSymbols(offset, symbols);

    for(auto& symbol: symbols) {
        callback(context.srcFile, symbol);
    }
}

async::Task<> Indexer::lookup(const HeaderContext& context,
                              uint64_t id,
                              llvm::StringRef name,
                              LookupCallback callback) {
    std::string indexPath = getIndexPath(context);
    if(indexPath.empty()) {
        co_return;
    }

    auto content = co_await async::fs::read(indexPath + ".sidx");
    if(!content) {
        log::warn("Failed to read index file: {}", indexPath + ".sidx");
        co_return;
    }

    llvm::SmallVector<index::SymbolIndex::Symbol, 8> symbols;
    index::SymbolIndex sindex(content->data(), content->size(), false);

    /// TODO: This task may be too slow, we may need to schedule it in thread pool.
    if(auto symbol = sindex.locateSymbol(id, name)) {
        callback(context.srcFile, *symbol);
    }
}

async::Task<> Indexer::lookup(llvm::StringRef file, uint32_t offset, LookupCallback callback) {
    HeaderContext context{.srcFile = file};

    /// Look up all symbol ids we need.
    llvm::SmallVector<SymbolID> symbols;

    co_await lookup(context,
                    offset,
                    [&](llvm::StringRef path, const index::SymbolIndex::Symbol& symbol) {
                        symbols.emplace_back(SymbolID{
                            .id = symbol.id(),
                            .name = symbol.name().str(),
                        });

                        callback(path, symbol);
                    });

    /// Look up all indices we have.
    for(auto& [path, tu]: tus) {
        if(path == file) {
            continue;
        }

        auto content = co_await async::fs::read(tu->indexPath + ".sidx");
        if(!content) {
            log::warn("Failed to read index file: {}", tu->indexPath + ".sidx");
            co_return;
        }

        index::SymbolIndex sindex(content->data(), content->size(), false);
        for(auto& symbol: symbols) {
            if(auto result = sindex.locateSymbol(symbol.id, symbol.name); result) {
                callback(path, *result);
            }
        }
    }

    for(auto& [path, header]: headers) {
        for(auto& index: header->indices) {
            auto content = co_await async::fs::read(index.path + ".sidx");
            if(!content) {
                log::warn("Failed to read index file: {}", index.path + ".sidx");
                co_return;
            }

            index::SymbolIndex sindex(content->data(), content->size(), false);
            for(auto& symbol: symbols) {
                if(auto result = sindex.locateSymbol(symbol.id, symbol.name); result) {
                    callback(path, *result);
                }
            }
        }
    }
}

async::Task<> Indexer::resolve(llvm::StringRef file, uint32_t offset, LookupCallback callback) {
    llvm::SmallVector<index::SymbolIndex::Symbol, 8> symbols;

    if(auto iter = tus.find(file); iter != tus.end()) {
        auto content = co_await async::fs::read(iter->second->indexPath + ".sidx");
        if(!content) {
            log::warn("Failed to read index file: {}", iter->second->indexPath + ".sidx");
            co_return;
        }

        index::SymbolIndex sindex(content->data(), content->size(), false);
        sindex.locateSymbols(offset, symbols);
        for(auto& symbol: symbols) {
            callback(file, symbol);
        }
        co_return;
    }

    if(auto iter = headers.find(file); iter != headers.end()) {
        auto header = iter->second;
        /// llvm::DenseSet<std::pair<uint64_t, std::string>> visited;
        for(auto& index: header->indices) {
            auto content = co_await async::fs::read(index.path + ".sidx");
            if(!content) {
                log::warn("Failed to read index file: {}", index.path + ".sidx");
                co_return;
            }

            index::SymbolIndex sindex(content->data(), content->size(), false);
            sindex.locateSymbols(offset, symbols);
            for(auto& symbol: symbols) {
                /// FIXME: USE a better way to avoid duplicated symbols.
                // if(visited.find({symbol.id(), symbol.name().str()}) == visited.end()) {
                //     visited.insert({symbol.id(), symbol.name().str()});
                //     callback(file, symbol);
                // }
            }
        }
        co_return;
    }
}

async::Task<proto::SemanticTokens> Indexer::semanticTokens(llvm::StringRef file) {
    auto indexPath = getIndexPath(file);

    auto content = co_await async::fs::read(indexPath + ".fidx");
    if(!content) {
        log::warn("Failed to read index file: {}", indexPath + ".fidx");
        co_return proto::SemanticTokens();
    }

    index::FeatureIndex findex(content->data(), content->size(), false);
    co_return proto::SemanticTokens{};
}

void Indexer::dumpForTest(llvm::StringRef file) {
    if(auto iter = tus.find(file); iter != tus.end()) {
        auto tu = iter->second;
    }

    if(auto iter = headers.find(file); iter != headers.end()) {
        auto header = iter->second;
        for(auto& index: header->indices) {
            auto buffer = llvm::MemoryBuffer::getFile(index.path + ".sidx");
            if(buffer) {
                auto content = buffer.get()->getBuffer();
                index::SymbolIndex sindex(const_cast<char*>(content.data()), content.size(), false);
                println("{}: {}", index.path, sindex.toJSON());
            }
        }
    }
}

void Indexer::saveToDisk() {
    auto path = path::join(options.dir, "index.json");
    std::error_code ec;
    llvm::raw_fd_ostream os(path, ec);
    if(ec) {
        log::warn("Failed to open index file: {} Beacuse {}", path, ec.message());
        return;
    }

    os << dumpToJSON();

    if(os.has_error()) {
        log::warn("Failed to write index file: {}", os.error().message());
    } else {
        log::info("Successfully saved index to disk");
    }
}

void Indexer::loadFromDisk() {
    auto path = path::join(options.dir, "index.json");
    auto file = llvm::MemoryBuffer::getFile(path);
    if(!file) {
        log::warn("Failed to open index file: {} Beacuse {}", path, file.getError());
        return;
    }

    auto json = json::parse(file.get()->getBuffer());
    ASSERT(json, "Failed to parse index file: {}", path);

    for(auto& value: *json->getAsObject()->getArray("tus")) {
        auto object = value.getAsObject();
        auto tu = new TranslationUnit{
            .srcPath = object->getString("srcPath")->str(),
            .indexPath = object->getString("indexPath")->str(),
            .mtime = std::chrono::milliseconds(*object->getInteger("mtime")),
            .locations = json::deserialize<std::vector<IncludeLocation>>(*object->get("locations")),
        };
        tus.try_emplace(tu->srcPath, tu);
    }

    /// All headers must be already initialized.
    for(auto& value: *json->getAsObject()->getArray("headers")) {
        auto object = value.getAsObject();
        llvm::StringRef srcPath = *object->getString("srcPath");

        Header* header = nullptr;
        if(auto iter = headers.find(srcPath); iter != headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = srcPath;
            headers.try_emplace(srcPath, header);
        }

        header->indices = json::deserialize<std::vector<HeaderIndex>>(*object->get("indices"));

        for(auto& value: *object->getArray("contexts")) {
            auto object = value.getAsObject();
            auto tu = tus[*object->getString("tu")];
            header->contexts[tu] =
                json::deserialize<std::vector<Context>>(*object->get("contexts"));
            tu->headers.insert(header);
        }
    }

    log::info("Successfully loaded index from disk");

    return;
}

}  // namespace clice
