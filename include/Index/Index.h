#pragma once

#include <clang/Index/IndexDataConsumer.h>

namespace clice {

class IndexConsumer : public clang::index::IndexDataConsumer {
public:
    void initialize(clang::ASTContext& Ctx) override {}

    void setPreprocessor(std::shared_ptr<clang::Preprocessor> PP) override {}

    bool handleDeclOccurrence(const clang::Decl* D,
                              clang::index::SymbolRoleSet Roles,
                              llvm::ArrayRef<clang::index::SymbolRelation> Relations,
                              clang::SourceLocation Loc,
                              clang::index::IndexDataConsumer::ASTNodeInfo ASTNode) override {
        return true;
    }
};

}  // namespace clice
