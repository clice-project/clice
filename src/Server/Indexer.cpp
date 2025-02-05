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

async::Task<Indexer::TranslationUnit*> Indexer::check(this Self& self, llvm::StringRef file) {
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

uint32_t Indexer::addIncludeChain(std::vector<Indexer::IncludeLocation>& locations,
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

        // if(header->srcPath == "/home/ykiko/C++/clice/include/Support/JSON.h") {
        //     if(index.symbol) {
        //         auto json = index.symbol->toJSON();
        //         llvm::SmallString<128> path;
        //         llvm::raw_svector_ostream stream(path);
        //         stream << json;
        //
        //        co_await async::fs::write(indices.back().path + ".json", path.data(),
        //        path.size());
        //    }
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

async::Task<proto::SemanticTokens> Indexer::semanticTokens(llvm::StringRef file) {
    std::string indexPath = "";

    if(auto iter = tus.find(file); iter != tus.end()) {
        indexPath = iter->second->indexPath;
    } else if(auto iter = headers.find(file); iter != headers.end()) {
        /// indexPath = iter->second->contexts.begin()->second.begin()->index->path;
    }

    if(indexPath.empty()) {
        co_return proto::SemanticTokens{};
    }

    auto content = co_await read(file);
    auto buffer = co_await read(indexPath + ".fidx");

    index::FeatureIndex index(const_cast<char*>(buffer->getBufferStart()),
                              buffer->getBufferSize(),
                              false);

    SourceConverter converter;
    co_return feature::toSemanticTokens(index.semanticTokens(),
                                        converter,
                                        content->getBuffer(),
                                        {});
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

async::Task<std::unique_ptr<llvm::MemoryBuffer>> Indexer::read(llvm::StringRef path) {
    co_return co_await async::submit([path] {
        auto file = llvm::MemoryBuffer::getFile(path);
        ASSERT(file, "Failed to open file: {}, because: {}", path, file.getError());
        return std::move(file.get());
    });
}

async::Task<> Indexer::lookup(llvm::ArrayRef<Indexer::SymbolID> ids,
                              RelationKind kind,
                              llvm::StringRef srcPath,
                              llvm::StringRef content,
                              std::string indexPath,
                              std::vector<proto::Location>& result) {
    auto indexFile = llvm::MemoryBuffer::getFile(indexPath);
    ASSERT(indexFile,
           "Failed to open index file: {}, Beacuse: {}",
           indexPath,
           indexFile.getError());
    index::SymbolIndex index(const_cast<char*>(indexFile.get()->getBufferStart()),
                             indexFile.get()->getBufferSize(),
                             false);

    for(auto& id: ids) {
        if(auto symbol = index.locateSymbol(id.id, id.name)) {
            for(auto relation: symbol->relations()) {
                if(relation.kind() & kind) {
                    auto range = relation.range();
                    auto begin = SourceConverter().toPosition(content, range->begin);
                    auto end = SourceConverter().toPosition(content, range->end);
                    result.emplace_back(proto::Location{
                        .uri = SourceConverter::toURI(srcPath),
                        .range = proto::Range{.start = begin, .end = end},
                    });
                }
            }
        }
    }

    co_return;
}

async::Task<std::vector<proto::Location>>
    Indexer::lookup(const proto::TextDocumentPositionParams& params, RelationKind kind) {
    auto srcPath = SourceConverter::toPath(params.textDocument.uri);
    llvm::StringRef indexPathPrefix = "";

    if(auto iter = tus.find(srcPath); iter != tus.end()) {
        indexPathPrefix = iter->second->indexPath;
    } else if(auto iter = headers.find(srcPath); iter != headers.end()) {
        /// FIXME: Indexer should use a variable to indicate know
        /// which context of the header is active. And use it to
        /// determine the index path prefix. Currently, we just
        /// use the first context.
        for(auto& [_, contexts]: iter->second->contexts) {
            for(auto& context: contexts) {
                // if(!context.indexPath.empty()) {
                //     indexPathPrefix = context.indexPath;
                //     break;
                // }
            }

            if(!indexPathPrefix.empty()) {
                break;
            }
        }
    }

    if(indexPathPrefix.empty()) {
        /// FIXME: If such index file does not exist, we should
        /// wait for the index task to complete.
        co_return proto::DefinitionResult{};
    }

    proto::DefinitionResult result;
    std::string indexPath = (indexPathPrefix + ".sidx").str();

    llvm::SmallVector<SymbolID, 4> ids;

    /// Lookup Target index first
    {
        auto srcFile = co_await read(srcPath);
        auto content = srcFile->getBuffer();
        auto offset = SourceConverter().toOffset(content, params.position);

        auto indexFile = co_await read(indexPath);
        index::SymbolIndex index(const_cast<char*>(indexFile.get()->getBufferStart()),
                                 indexFile.get()->getBufferSize(),
                                 false);
        llvm::SmallVector<index::SymbolIndex::Symbol> symbols;
        index.locateSymbols(offset, symbols);

        for(auto& symbol: symbols) {
            ids.emplace_back(SymbolID{symbol.id(), symbol.name().str()});

            for(auto relation: symbol.relations()) {
                if(relation.kind() & kind) {
                    auto range = relation.range();
                    auto begin = SourceConverter().toPosition(content, range->begin);
                    auto end = SourceConverter().toPosition(content, range->end);
                    result.emplace_back(proto::Location{
                        .uri = SourceConverter::toURI(srcPath),
                        .range = proto::Range{.start = begin, .end = end},
                    });
                }
            }
        }
    }

    for(auto& [path, tu]: tus) {
        if(path == srcPath || tu->indexPath.empty()) {
            continue;
        }

        auto srcPath = path.str();
        auto srcFile = co_await read(srcPath);
        co_await lookup(ids, kind, srcPath, srcFile->getBuffer(), tu->indexPath + ".sidx", result);
    }

    llvm::StringSet<> visited;
    for(auto& [path, header]: headers) {
        if(path == srcPath) {
            continue;
        }

        auto srcPath = path.str();
        auto srcFile = co_await read(srcPath);
        auto content = srcFile->getBuffer();

        for(auto& [_, contexts]: header->contexts) {
            // for(auto& context: contexts) {
            //     if(context.indexPath.empty() || visited.contains(context.indexPath)) {
            //         continue;
            //     }
            //
            //    visited.insert(context.indexPath);
            //    co_await lookup(ids, kind, srcPath, content, context.indexPath + ".sidx", result);
            //}
        }
    }

    co_await async::submit([&] {
        ranges::sort(result, refl::less);
        auto [first, last] = ranges::unique(result, refl::equal);
        result.erase(first, last);
    });

    co_return result;
}

async::Task<proto::CallHierarchyIncomingCallsResult>
    Indexer::incomingCalls(const proto::CallHierarchyIncomingCallsParams& params) {
    co_return proto::CallHierarchyIncomingCallsResult{};
}

async::Task<proto::CallHierarchyOutgoingCallsResult>
    Indexer::outgoingCalls(const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return proto::CallHierarchyOutgoingCallsResult{};
}

async::Task<proto::TypeHierarchySupertypesResult>
    Indexer::supertypes(const proto::TypeHierarchySupertypesParams& params) {
    co_return proto::TypeHierarchySupertypesResult{};
}

async::Task<proto::TypeHierarchySubtypesResult>
    Indexer::subtypes(const proto::TypeHierarchySubtypesParams& params) {
    co_return proto::TypeHierarchySubtypesResult{};
}

}  // namespace clice
