#include "AST/FilterASTVisitor.h"
#include "AST/Utility.h"
#include "Compiler/Compilation.h"
#include "Feature/DocumentSymbol.h"
#include "Support/Ranges.h"
#include "Support/Compare.h"

namespace clice::feature {

namespace {

/// Use DFS to traverse the AST and collect document symbols.
class DocumentSymbolCollector : public FilteredASTVisitor<DocumentSymbolCollector> {

public:
    using Base = FilteredASTVisitor<DocumentSymbolCollector>;

    DocumentSymbolCollector(CompilationUnit& unit, bool interested_only) :
        Base(unit, interested_only) {}

    bool is_interested(clang::Decl* decl) {
        switch(decl->getKind()) {
            case clang::Decl::Namespace:
            case clang::Decl::Enum:
            case clang::Decl::EnumConstant:
            case clang::Decl::Function:
            case clang::Decl::CXXMethod:
            case clang::Decl::CXXConstructor:
            case clang::Decl::CXXDestructor:
            case clang::Decl::CXXConversion:
            case clang::Decl::CXXDeductionGuide:
            case clang::Decl::Record:
            case clang::Decl::CXXRecord:
            case clang::Decl::Field:
            case clang::Decl::Var:
            case clang::Decl::Binding:
            case clang::Decl::Concept: {
                return true;
            }

            default: {
                return false;
            }
        }
    }

    bool on_traverse_decl(clang::Decl* decl, auto MF) {
        if(!is_interested(decl)) {
            return (this->*MF)(decl);
        }

        auto ND = llvm::cast<clang::NamedDecl>(decl);
        auto [fid, selectionRange] =
            unit.decompose_range(unit.expansion_location(ND->getLocation()));

        auto& frame = interested_only ? result : sharedResult[fid];
        auto cursor = frame.cursor;

        /// Add new symbol.
        auto& symbol = frame.cursor->emplace_back();
        symbol.kind = SymbolKind::from(decl);
        symbol.name = ast::name_of(ND);
        symbol.selectionRange = selectionRange;
        symbol.range = selectionRange;

        /// Adjust the node.
        frame.cursor = &symbol.children;

        bool res = (this->*MF)(decl);

        /// When all children node are set, go back to last node.
        (interested_only ? result : sharedResult[fid]).cursor = cursor;

        return res;
    }

public:
    struct SymbolFrame {
        DocumentSymbols symbols;
        DocumentSymbols* cursor = &symbols;
    };

    SymbolFrame result;
    index::Shared<SymbolFrame> sharedResult;
};

}  // namespace

DocumentSymbols document_symbols(CompilationUnit& unit) {
    DocumentSymbolCollector collector(unit, true);
    collector.TraverseDecl(unit.tu());

    auto& frame = collector.result;
    ranges::sort(frame.symbols, refl::less);
    return std::move(frame.symbols);
}

index::Shared<DocumentSymbols> index_document_symbol(CompilationUnit& unit) {
    DocumentSymbolCollector collector(unit, true);
    collector.TraverseDecl(unit.tu());

    index::Shared<DocumentSymbols> result;
    for(auto& [fid, frame]: collector.sharedResult) {
        ranges::sort(frame.symbols, refl::less);
        result.try_emplace(fid, std::move(frame.symbols));
    }
    return result;
}

}  // namespace clice::feature
