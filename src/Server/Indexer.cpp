#include <random>

#include "Support/Assert.h"
#include "Compiler/Compilation.h"
#include "Index/SymbolIndex.h"
#include "Server/Logger.h"
#include "Server/Indexer.h"

namespace clice {

static uint32_t addIncludeChain(std::vector<Indexer::IncludeLocation>& locations,
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
    locations[index].filename = path::real_path(path::real_path(entry->getName()));

    if(auto presumed = SM.getPresumedLoc(SM.getIncludeLoc(fid), false); presumed.isValid()) {
        locations[index].line = presumed.getLine();
        auto include = addIncludeChain(locations, files, SM, presumed.getFileID());
        locations[index].include = include;
    }

    return index;
}

Indexer::~Indexer() {
    for(auto& [_, header]: headers) {
        delete header;
    }

    for(auto& [_, tu]: tus) {
        delete tu;
    }
}

async::Task<bool> Indexer::needUpdate(TranslationUnit* tu) {
    auto stats = co_await async::stat(tu->srcPath);
    /// If the file is modified, we need to update the index.
    if(stats.mtime > tu->mtime) {
        co_return true;
    }

    /// Check all headers.
    for(auto& header: tu->headers) {
        auto stats = co_await async::stat(header->srcPath);
        if(stats.mtime > tu->mtime) {
            co_return true;
        }
    }

    co_return false;
}

async::Task<> Indexer::merge(Header* header) {
    co_await async::submit([header] {
        llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> indices;

        for(auto& [tu, contexts]: header->contexts) {
            for(auto& context: contexts) {
                if(context.indexPath.empty()) {
                    continue;
                }

                auto file = llvm::MemoryBuffer::getFile(context.indexPath + ".sidx");
                if(!file) {
                    log::warn("Failed to open index file: {}", context.indexPath);
                    continue;
                }

                bool merged = false;
                for(auto& [indexPath, value]: indices) {
                    if(file->get()->getBuffer() == value->getBuffer()) {
                        auto error = fs::remove(context.indexPath + ".sidx");
                        log::info("Merged index file: {} -> {}", context.indexPath, indexPath);
                        context.indexPath = indexPath;
                        merged = true;
                        break;
                    }
                }

                if(!merged) {
                    indices.try_emplace(context.indexPath, std::move(file.get()));
                }
            }
        }
    });

    co_return;
}

async::Task<> Indexer::index(llvm::StringRef file) {
    auto real_path = path::real_path(file);
    file = real_path;

    TranslationUnit* tu = nullptr;
    bool needIndex = false;

    if(auto iter = tus.find(file); iter != tus.end()) {
        /// If no need to update, just return.
        if(!co_await needUpdate(iter->second)) {
            log::info("No need to update index for file: {}", file);
            co_return;
        }

        /// Otherwise, we need to update the translation unit.
        /// Clear all old header contexts associated with this translation unit.
        tu = iter->second;
        needIndex = true;
        for(auto& header: tu->headers) {
            header->contexts[tu].clear();
        }
    } else {
        tu = new TranslationUnit{
            .srcPath = file.str(),
            .indexPath = "",
        };
        needIndex = true;
        tus.try_emplace(file, tu);
    }

    if(!needIndex) {
        log::info("No need to update index for file: {}", file);
        co_return;
    }

    auto command = database.getCommand(file);
    if(command.empty()) {
        log::warn("No command found for file: {}", file);
        co_return;
    }

    CompilationParams params;
    params.command = command;
    auto info = co_await async::submit([&params] { return compile(params); });

    if(!info) {
        log::warn("Failed to compile {}: {}", file, info.takeError());
        co_return;
    }

    /// Update all headers and translation units.
    auto& SM = info->srcMgr();
    std::vector<IncludeLocation> locations;
    llvm::DenseMap<clang::FileID, uint32_t> files;

    for(auto& [fid, directive]: info->directives()) {
        for(auto& include: directive.includes) {
            /// If the include is invalid, it indicates that the file is skipped because of
            /// include guard, or `#pragma once`. Such file cannot provide header context.
            /// So we just skip it.
            if(include.fid.isInvalid()) {
                continue;
            }

            /// Add all include chains.
            addIncludeChain(locations, files, SM, include.fid);
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
        if(auto iter = headers.find(name); iter != headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = name;
            headers.try_emplace(name, header);
        }

        header->contexts[tu].emplace_back(Context{
            .indexPath = "",
            .include = include,
        });
    }

    auto indices = co_await async::submit([&info] { return index::index(*info); });

    /// Write binary index to file.
    for(auto& [fid, index]: indices) {
        if(fid == SM.getMainFileID()) {
            if(tu->indexPath.empty()) {
                tu->indexPath = getIndexPath(tu->srcPath);
            }
            co_await async::write(tu->indexPath + ".sidx",
                                  static_cast<char*>(index.base),
                                  index.size);
            continue;
        }

        auto entry = SM.getFileEntryRefForID(fid);
        if(!entry) {
            continue;
        }
        assert(entry && "Invalid file entry");
        auto name = path::real_path(entry->getName());
        auto include = files[fid];
        assert(headers.contains(name) && "Header not found");
        for(auto& context: headers[name]->contexts[tu]) {
            if(context.include == include) {
                if(context.indexPath.empty()) {
                    context.indexPath = getIndexPath(name);
                }
                co_await async::write(context.indexPath + ".sidx",
                                      static_cast<char*>(index.base),
                                      index.size);
            }
        }
    }

    co_return;
}

async::Task<> Indexer::index(llvm::StringRef file, ASTInfo& info) {
    co_return;
}

async::Task<> Indexer::indexAll() {
    auto total = database.size();
    auto count = 0;

    std::vector<async::Task<>> tasks;

    for(auto& [file, command]: database) {
        tasks.emplace_back(index(file));

        if(tasks.size() == 4) {
            co_await async::gather(tasks[0], tasks[1], tasks[2], tasks[3]);
            tasks.clear();
            count += 4;
            log::info("Indexing progress: {0}/{1}", count, total);
        }
    }

    tasks.clear();
    for(auto& [path, header]: headers) {
        tasks.emplace_back(merge(header));

        if(tasks.size() == 4) {
            co_await async::gather(tasks[0], tasks[1], tasks[2], tasks[3]);
            tasks.clear();
        }
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
            {"srcPath",  header->srcPath    },
            {"contexts", std::move(contexts)},
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
        {"headers", std::move(headers)},
        {"tus",     std::move(tus)    },
    };
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
                if(!context.indexPath.empty()) {
                    indexPathPrefix = context.indexPath;
                    break;
                }
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
            for(auto& context: contexts) {
                if(context.indexPath.empty() || visited.contains(context.indexPath)) {
                    continue;
                }

                visited.insert(context.indexPath);
                co_await lookup(ids, kind, srcPath, content, context.indexPath + ".sidx", result);
            }
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
