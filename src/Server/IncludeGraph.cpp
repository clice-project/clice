#include <random>

#include "Server/IncludeGraph.h"
#include "Index/SymbolIndex.h"
#include "Index/FeatureIndex.h"
#include "Index/Shared2.h"
#include "Support/Compare.h"
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
                                       clang::FileID fid,
                                       ASTInfo& AST) {
    auto [iter, success] = files.try_emplace(fid, locations.size());
    if(!success) {
        return iter->second;
    }

    auto index = iter->second;

    auto includeLoc = SM.getIncludeLoc(fid);
    if(includeLoc.isValid()) {
        auto presumed = SM.getPresumedLoc(includeLoc, false);
        locations.emplace_back();
        locations[index].line = presumed.getLine();

        auto path = AST.getFilePath(presumed.getFileID());
        auto [iter, success] = pathIndices.try_emplace(path, pathPool.size());
        if(success) {
            pathPool.emplace_back(path);
        }
        locations[index].file = iter->second;

        uint32_t include = -1;
        if(presumed.getIncludeLoc().isValid()) {
            include =
                addIncludeChain(locations, files, SM, SM.getFileID(presumed.getIncludeLoc()), AST);
        }
        locations[index].include = include;
    }

    return index;
}

void IncludeGraph::addContexts(ASTInfo& AST,
                               TranslationUnit* tu,
                               llvm::DenseMap<clang::FileID, uint32_t>& files) {
    auto& SM = AST.srcMgr();

    std::vector<IncludeLocation> locations;

    for(auto& [fid, directive]: AST.directives()) {
        for(auto& include: directive.includes) {
            /// If the include is invalid, it indicates that the file is skipped because of
            /// include guard, or `#pragma once`. Such file cannot provide header context.
            /// So we just skip it.
            if(include.skipped) {
                continue;
            }

            /// Add all include chains.
            addIncludeChain(locations, files, SM, include.fid, AST);
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

        auto path = AST.getFilePath(fid);

        Header* header = nullptr;
        if(auto iter = headers.find(path); iter != headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = path;
            headers.try_emplace(path, header);
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
    auto indices = co_await async::submit([&info] { return index::Index2::build(info); });

    auto& SM = info.srcMgr();

    for(auto& [fid, index]: *indices) {
        if(fid == SM.getMainFileID()) {
            if(tu->indexPath.empty()) {
                tu->indexPath = getIndexPath(tu->srcPath);
            }

            co_await index.write(tu->indexPath);

            continue;
        }

        auto name = info.getFilePath(fid);
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

        co_await index.write(indices.back().path);
    }
}

}  // namespace clice
