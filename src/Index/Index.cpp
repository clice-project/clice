#include "AST/Semantic.h"
#include "Index/Index.h"
#include "Index/IncludeGraph.h"
#include "Support/Format.h"

namespace clice::index::memory {

namespace {

class IndexBuilder : public Indices, public SemanticVisitor<IndexBuilder> {
public:
    IndexBuilder(ASTInfo& AST) : SemanticVisitor(AST, false) {
        tu_index = std::make_unique<TUIndex>();
        tu_index->path = AST.getFilePath(SM.getMainFileID());
        tu_index->content = AST.getFileContent(SM.getMainFileID());
        tu_index->graph = IncludeGraph::from(AST);
    }

    RawIndex& getIndex(clang::FileID fid) {
        if(fid == SM.getMainFileID()) {
            return *tu_index;
        }

        if(auto it = header_indices.find(fid); it != header_indices.end()) {
            return *it->second;
        }

        auto [it, _] = header_indices.try_emplace(fid, new RawIndex());
        auto& index = *it->second;
        index.path = AST.getFilePath(fid);
        index.content = AST.getFileContent(fid);
        return index;
    }

    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        assert(decl && "Invalid decl");
        decl = normalize(decl);

        if(location.isMacroID()) {
            auto spelling = AST.getSpellingLoc(location);
            auto expansion = AST.getExpansionLoc(location);

            /// FIXME: For location from macro, we only handle the case that the
            /// spelling and expansion are in the same file currently.
            if(AST.getFileID(spelling) != AST.getFileID(expansion)) {
                return;
            }

            /// For occurrence, we always use spelling location.
            location = spelling;
        }

        auto [fid, range] = AST.toLocalRange(location);
        auto& index = getIndex(fid);
        auto symbol_id = AST.getSymbolID(decl);
        auto& symbol = index.get_symbol(symbol_id.hash);
        symbol.kind = SymbolKind::from(decl);
        index.add_occurrence(range, symbol_id.hash);
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// FIXME: Figure out when location is MacroID.
        if(location.isMacroID()) {
            return;
        }

        auto [fid, range] = AST.toLocalRange(location);
        auto& index = getIndex(fid);
        auto symbol_id = AST.getSymbolID(def);
        auto& symbol = index.get_symbol(symbol_id.hash);
        symbol.kind = SymbolKind::Macro;
        symbol.name = getTokenSpelling(SM, def->getDefinitionLoc());
        index.add_occurrence(range, symbol_id.hash);

        if(kind & RelationKind::Definition) {
            auto begin = def->getDefinitionLoc();
            auto end = def->getDefinitionEndLoc();
            assert(begin.isFileID() && end.isFileID() && "Invalid location");
            auto [fid2, definition_range] = AST.toLocalRange(clang::SourceRange(begin, end));
            assert(fid == fid2 && "Invalid macro definition location");
            /// definitionLoc = builder.getLocation(range);

            index.add_relation(symbol,
                               Relation{
                                   .kind = RelationKind::Definition,
                                   .range = range,
                                   .definition_range = definition_range,
                               });
        } else {
            index.add_relation(symbol,
                               Relation{
                                   .kind = RelationKind::Reference,
                                   .range = range,
                                   .target_symbol = 0,
                               });
        }
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        auto [fid, relationRange] = AST.toLocalExpansionRange(range);

        Relation relation{.kind = kind};

        if(kind.isDeclOrDef()) {
            auto [fid2, definitionRange] = AST.toLocalExpansionRange(decl->getSourceRange());
            assert(fid == fid2 && "Invalid definition location");
            relation.range = relationRange;
            relation.definition_range = definitionRange;
        } else if(kind.isReference()) {
            relation.range = relationRange;
            relation.target_symbol = 0;
        } else if(kind.isBetweenSymbol()) {
            auto symbol_id = AST.getSymbolID(normalize(target));
            relation.target_symbol = symbol_id.hash;
        } else if(kind.isCall()) {
            auto symbol_id = AST.getSymbolID(normalize(target));
            relation.range = relationRange;
            relation.target_symbol = symbol_id.hash;
        } else {
            std::unreachable();
        }

        auto& index = getIndex(fid);
        auto symbol_id = AST.getSymbolID(normalize(decl));
        auto& symbol = index.get_symbol(symbol_id.hash);
        index.add_relation(symbol, relation);
    }
};

}  // namespace

Indices index(ASTInfo& AST) {
    IndexBuilder builder(AST);
    builder.run();
    return std::move(builder);
}

}  // namespace clice::index::memory
