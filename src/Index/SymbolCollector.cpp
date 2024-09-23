#include <Index/SymbolCollector.h>

namespace clice {

bool SymbolCollector::handleModuleOccurrence(const clang::ImportDecl* decl,
                                             const clang::Module* module,
                                             clang::index::SymbolRoleSet roles,
                                             clang::SourceLocation location) {
    return true;
}

bool SymbolCollector::handleMacroOccurrence(const clang::IdentifierInfo* name,
                                            const clang::MacroInfo* info,
                                            clang::index::SymbolRoleSet roles,
                                            clang::SourceLocation location) {
    return true;
}

bool SymbolCollector::handleDeclOccurrence(const clang::Decl* canonical,
                                           clang::index::SymbolRoleSet roles,
                                           llvm::ArrayRef<clang::index::SymbolRelation> relations,
                                           clang::SourceLocation location,
                                           clang::index::IndexDataConsumer::ASTNodeInfo node) {
    return true;
}

}  // namespace clice
