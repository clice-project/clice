#include "Index/Index2.h"
#include "AST/Semantic.h"

namespace clice::index::memory2 {

class SymbolIndexBuilder : public SemanticVisitor<SymbolIndexBuilder> {
public:
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
        auto& index = indices[fid];
        auto symbol_id = AST.getSymbolID(decl);
        /// auto symbol = index.getSymbol(symbol_id);
        /// index.addOccurrence(range, symbol);
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// FIXME: Figure out when location is MacroID.
        if(location.isMacroID()) {
            return;
        }

        auto [fid, range] = AST.toLocalRange(location);
        auto& index = indices[fid];
        auto symbol_id = AST.getSymbolID(def);
        /// auto symbol = index.getSymbol(symbol_id);
        /// index.addOccurrence(range, symbol);

        /// If the macro is a definition, set definition range for it.
        std::uint32_t definitionLoc = std::numeric_limits<std::uint32_t>::max();

        if(kind & RelationKind::Definition) {
            auto begin = def->getDefinitionLoc();
            auto end = def->getDefinitionEndLoc();
            assert(begin.isFileID() && end.isFileID() && "Invalid location");
            auto [fid2, range] = AST.toLocalRange(clang::SourceRange(begin, end));
            assert(fid == fid2 && "Invalid macro definition location");
            /// definitionLoc = builder.getLocation(range);
        }
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {}

private:
    llvm::DenseMap<clang::FileID, SymbolIndex> indices;
};

}  // namespace clice::index::memory2
