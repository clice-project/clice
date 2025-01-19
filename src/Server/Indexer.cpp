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
    auto presumed = SM.getPresumedLoc(SM.getIncludeLoc(fid), false);

    locations[index].filename = entry->getName();
    locations[index].line = presumed.getLine();

    if(presumed.getFileID().isValid()) {
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
            files[fid] = addIncludeChain(locations, files, SM, include.fid);
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
    }

    /// Update the translation unit.
    tu->locations = std::move(locations);

    /// Update the header context.
    for(auto& [fid, include]: files) {
        auto entry = SM.getFileEntryRefForID(fid);
        assert(entry && "Invalid file entry");
        auto name = entry->getName();

        Header* header = nullptr;
        if(auto iter = headers.find(name); iter != headers.end()) {
            header = iter->second;
        } else {
            header = new Header;
            header->srcPath = name;
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
            tu->index = "111";
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
                context.index = "111";
            }
        }
    }

    co_return;
}

async::Task<> Indexer::index(llvm::StringRef file, ASTInfo& info) {
    co_return;
}

}  // namespace clice
