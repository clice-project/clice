#include <Index/CSIF.h>

namespace clice {

/// resopnsible for owning the symbols.
class SymbolSlab {
    struct SymbolInformation {};

public:
    CSIF toCSIF() {
        // NOTE: we assign the relations to the symbols here rather than construct
        // symbol. Beacuse the reference is possible to be invalid after the append
        // operation.
        for(auto& symbol: symbols) {
            symbol.relations = relations.at(symbol.id);
        }
        CSIF result;
        result.symbols = symbols;
        result.occurrences = occurrences;
        return result;
    }

    SymbolSlab& addSymbol();

    SymbolSlab& addDefinition();

    SymbolSlab& addDeclaration();

    SymbolSlab& addTypeDefinition();

    SymbolSlab& addRef();

private:
    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
    llvm::DenseMap<SymbolID, std::size_t> symbolIDs;
    llvm::DenseMap<SymbolID, std::vector<Relation>> relations;
};

}  // namespace clice
