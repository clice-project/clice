#pragma once

#include <clang/Index/IndexDataConsumer.h>
#include <clang/Index/IndexingAction.h>

namespace clice {

class SymbolSlab;

class SymbolCollector : public clang::index::IndexDataConsumer {
public:
    SymbolCollector(SymbolSlab& slab) : slab(slab) {}

    void initialize(clang::ASTContext& context) override {}

    void setPreprocessor(std::shared_ptr<clang::Preprocessor> preproc) override { this->preproc = preproc; }

    bool handleModuleOccurrence(const clang::ImportDecl* decl,
                                const clang::Module* module,
                                clang::index::SymbolRoleSet roles,
                                clang::SourceLocation location) override;

    bool handleMacroOccurrence(const clang::IdentifierInfo* name,
                               const clang::MacroInfo* info,
                               clang::index::SymbolRoleSet roles,
                               clang::SourceLocation location) override;

    bool handleDeclOccurrence(const clang::Decl* canonical,
                              clang::index::SymbolRoleSet roles,
                              llvm::ArrayRef<clang::index::SymbolRelation> relations,
                              clang::SourceLocation location,
                              clang::index::IndexDataConsumer::ASTNodeInfo node) override;

private:
    SymbolSlab& slab;
    std::shared_ptr<clang::Preprocessor> preproc;
};

}  // namespace clice
