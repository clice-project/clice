#include "Index/IncludeGraph.h"
#include "Compiler/AST.h"

namespace clice::index {

static std::uint32_t addIncludeChain(ASTInfo& AST,
                                     clang::FileID fid,
                                     IncludeGraph& graph,
                                     llvm::StringMap<std::uint32_t>& path_table) {
    auto& SM = AST.srcMgr();
    auto& [paths, locations, file_table] = graph;
    auto [iter, success] = file_table.try_emplace(fid, locations.size());
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
        auto [iter, success] = path_table.try_emplace(path, paths.size());
        if(success) {
            paths.emplace_back(path);
        }
        locations[index].path = iter->second;

        uint32_t include = -1;
        if(presumed.getIncludeLoc().isValid()) {
            include =
                addIncludeChain(AST, SM.getFileID(presumed.getIncludeLoc()), graph, path_table);
        }
        locations[index].include = include;
    }

    return index;
}

IncludeGraph IncludeGraph::from(ASTInfo& AST) {
    llvm::StringMap<std::uint32_t> path_table;
    IncludeGraph graph;
    for(auto fid: AST.files()) {
        addIncludeChain(AST, fid, graph, path_table);
    }
    return graph;
}

}  // namespace clice::index
