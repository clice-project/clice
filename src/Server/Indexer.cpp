#include <random>

#include "Compiler/Compilation.h"
#include "Index/SymbolIndex.h"
#include "Server/Logger.h"
#include "Server/Indexer.h"

namespace clice {

static long long generate_unique_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000);
    return ms + dis(gen);
}

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
    locations[index].filename = entry->getName();

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

async::Task<> Indexer::index(llvm::StringRef file) {
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

    TranslationUnit* tu = nullptr;
    if(auto iter = tus.find(file); iter != tus.end()) {
        tu = iter->second;
        /// Clear all contexts associated with this translation unit.
        for(auto& header: tu->headers) {
            header->contexts[tu].clear();
        }
    } else {
        tu = new TranslationUnit{
            .srcPath = file.str(),
            .index = "",
        };
        tus.try_emplace(file, tu);
    }

    /// Update the translation unit.
    tu->locations = std::move(locations);

    /// Update the header context.
    for(auto& [fid, include]: files) {
        if(fid == SM.getMainFileID()) {
            continue;
        }

        auto entry = SM.getFileEntryRefForID(fid);
        assert(entry && "Invalid file entry");
        auto name = entry->getName();

        Header* header = nullptr;
        if(auto iter = headers.find(name); iter != headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = name;
            headers.try_emplace(name, header);
        }

        header->contexts[tu].emplace_back(Context{
            .index = "",
            .include = include,
        });
    }

    auto indices = co_await async::submit([&info] { return index::index(*info); });

    /// Write binary index to file.
    for(auto& [fid, index]: indices) {
        if(fid == SM.getMainFileID()) {
            tu->index = path::join(options.dir,
                                   path::filename(tu->srcPath) + "." +
                                       llvm::Twine(generate_unique_id()) + ".sidx");
            co_await async::write(tu->index, static_cast<char*>(index.base), index.size);
            continue;
        }

        auto entry = SM.getFileEntryRefForID(fid);
        assert(entry && "Invalid file entry");
        auto name = entry->getName();
        auto include = files[fid];
        assert(headers.contains(name) && "Header not found");
        for(auto& context: headers[name]->contexts[tu]) {
            if(context.include == include) {
                /// Write index to binary file.
                context.index = path::join(options.dir,
                                           path::filename(name) + "." +
                                               llvm::Twine(generate_unique_id()) + ".fidx");
                co_await async::write(context.index, static_cast<char*>(index.base), index.size);
            }
        }
    }

    co_return;
}

async::Task<> Indexer::index(llvm::StringRef file, ASTInfo& info) {
    co_return;
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
            {"index",     tu->index                     },
            {"locations", json::serialize(tu->locations)},
        });
    }

    return json::Object{
        {"headers", std::move(headers)},
        {"tus",     std::move(tus)    },
    };
}

}  // namespace clice
