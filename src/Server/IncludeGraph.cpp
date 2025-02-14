#include <random>

#include "Server/IncludeGraph.h"
#include "Index/SymbolIndex.h"
#include "Index/FeatureIndex.h"
#include "Support/Logger.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/xxhash.h"

namespace clice {

IncludeGraph::~IncludeGraph() {
    for(auto& [_, header]: headers) {
        delete header;
    }

    for(auto& [_, tu]: tus) {
        delete tu;
    }
}

json::Value IncludeGraph::dump() {
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

void IncludeGraph::load(const json::Value& json) {
    auto object = json.getAsObject();

    for(auto& value: *object->getArray("tus")) {
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
    for(auto& value: *object->getArray("headers")) {
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

    pathPool = json::deserialize<std::vector<std::string>>(*object->get("paths"));
    for(std::size_t i = 0; i < pathPool.size(); i++) {
        pathIndices.try_emplace(pathPool[i], i);
    }
}

async::Task<> IncludeGraph::index(llvm::StringRef file, CompilationDatabase& database) {
    auto path = path::real_path(file);
    file = path;

    auto tu = co_await check(file);
    if(!tu) {
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
        log::warn("Failed to compile {}: {}", file, info.error());
        co_return;
    }

    llvm::DenseMap<clang::FileID, uint32_t> files;

    /// Otherwise, we need to update all header contexts.
    addContexts(**info, tu, files);

    co_await updateIndices(**info, tu, files);
}

std::string IncludeGraph::getIndexPath(llvm::StringRef file) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000);
    return path::join(options.dir, path::filename(file) + "." + llvm::Twine(ms + dis(gen)));
}

async::Task<TranslationUnit*> IncludeGraph::check(llvm::StringRef file) {
    auto iter = tus.find(file);

    /// If no translation unit found, we need to create a new one.
    if(iter == tus.end()) {
        auto tu = new TranslationUnit;
        tu->srcPath = file.str();
        tus.try_emplace(file, tu);
        co_return tu;
    }

    auto tu = iter->second;

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

uint32_t IncludeGraph::addIncludeChain(std::vector<IncludeLocation>& locations,
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

void IncludeGraph::addContexts(ASTInfo& info,
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

async::Task<> IncludeGraph::updateIndices(ASTInfo& info,
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

    auto& SM = info.srcMgr();

    for(auto& [fid, index]: *indices) {
        if(fid == SM.getMainFileID()) {
            if(tu->indexPath.empty()) {
                tu->indexPath = getIndexPath(tu->srcPath);
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
        assert(headers.contains(name) && "Invalid header name");

        auto header = headers[name];

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
            .path = getIndexPath(header->srcPath),
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

}  // namespace clice
