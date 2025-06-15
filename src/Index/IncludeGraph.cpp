#include "Index/IncludeGraph.h"
#include "Compiler/AST.h"

namespace clice::index {

static std::uint32_t addIncludeChain(CompilationUnit& unit,
                                     clang::FileID fid,
                                     IncludeGraph& graph,
                                     llvm::StringMap<std::uint32_t>& path_table) {
    auto& [paths, locations, file_table] = graph;
    auto [iter, success] = file_table.try_emplace(fid, locations.size());
    if(!success) {
        return iter->second;
    }

    auto index = iter->second;

    auto includeLoc = unit.include_location(fid);
    if(includeLoc.isValid()) {
        auto presumed = unit.presumed_location(includeLoc);
        locations.emplace_back();
        locations[index].line = presumed.getLine();

        auto path = unit.getFilePath(presumed.getFileID());
        auto [iter, success] = path_table.try_emplace(path, paths.size());
        if(success) {
            paths.emplace_back(path);
        }
        locations[index].path = iter->second;

        uint32_t include = -1;
        if(presumed.getIncludeLoc().isValid()) {
            include =
                addIncludeChain(unit, unit.file_id(presumed.getIncludeLoc()), graph, path_table);
        }
        locations[index].include = include;
    }

    return index;
}

IncludeGraph IncludeGraph::from(CompilationUnit& unit) {
    llvm::StringMap<std::uint32_t> path_table;
    IncludeGraph graph;
    for(auto fid: unit.files()) {
        addIncludeChain(unit, fid, graph, path_table);
    }
    return graph;
}

}  // namespace clice::index
