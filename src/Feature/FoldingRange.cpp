#include "Feature/FoldingRange.h"
#include "Compiler/Compiler.h"

namespace clice {

namespace {

struct FoldingRangeCollector : public clang::RecursiveASTVisitor<FoldingRangeCollector> {

    proto::FoldingRangeResult result;

    bool VisitNamespaceDecl(clang::NamespaceDecl* ND) {
        return true;
    }

    bool VisitRecordDecl(clang::RecordDecl* RD) {
        return true;
    }

    bool VisitCXXRecordDecl(clang::CXXRecordDecl* RD) {
        return true;
    }
};

}  // namespace

namespace feature {

proto::FoldingRangeResult foldingRange(FoldingRangeParams& _, ASTInfo& ast) {
    FoldingRangeCollector collector;
    collector.VisitTranslationUnitDecl(ast.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
