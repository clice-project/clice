#pragma once

#include <clang/Index/IndexDataConsumer.h>
#include <clang/Index/IndexingAction.h>

namespace clice {

class IndexConsumer : public clang::index::IndexDataConsumer {
private:
    std::shared_ptr<clang::Preprocessor> PP;

public:
    void initialize(clang::ASTContext& Ctx) override {}

    void setPreprocessor(std::shared_ptr<clang::Preprocessor> PP) override { this->PP = PP; }

    bool handleDeclOccurrence(const clang::Decl* D,
                              clang::index::SymbolRoleSet Roles,
                              llvm::ArrayRef<clang::index::SymbolRelation> Relations,
                              clang::SourceLocation Loc,
                              clang::index::IndexDataConsumer::ASTNodeInfo ASTNode) override {
        llvm::outs() << "------------------------------------------------\n";
        // ASTNode.OrigD->dump();
        Loc.dump(PP->getSourceManager());
        clang::index::printSymbolRoles(Roles, llvm::outs());
        llvm::outs() << "\n";

        for(auto& R: Relations) {
            R.RelatedSymbol->dump();
            clang::index::printSymbolRoles(R.Roles, llvm::outs());
            llvm::outs() << "\n";
        }

        clang::ClassTemplateSpecializationDecl* decl;

        // ASTNode.OrigD->dump();

        llvm::outs() << "\n";
        return true;
    }
};

}  // namespace clice
