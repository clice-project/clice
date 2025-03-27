#include "Feature/InlayHint2.h"
#include "AST/FilterASTVisitor.h"

namespace clice::feature {

namespace {

class InlayHintsCollector : FilteredASTVisitor<InlayHintsCollector> {
public:
    using Base = FilteredASTVisitor<InlayHintsCollector>;
    using Base::Base;

private:
    void addInlayHint(InlayHintKind kind, clang::SourceLocation location) {
        auto [fid, offset] = AST.getDecomposedLoc(location);
        auto& hints = interestedOnly ? result : sharedResult[fid];
    }

    /// For `auto x|:int| = 1` or `std::vector|<int>| vec = {1, 2, 3}`.
    bool VisitVarDecl(const clang::VarDecl* decl) {
        /// Handle only VarDecl. FIXME: VarTemplateSpecialization?
        if(decl->getKind() != clang::Decl::Var) {
            return true;
        }

        auto type = decl->getType();
        if(type.isNull() || !type->isUndeducedAutoType() || type->isDependentType()) {
            return true;
        }

        /// TODO:
        /// type->getContainedDeducedType();

        return true;
    }

    /// For `auto [a|:int|, b|:int|] = std::tuple(1, 2)`.
    bool VisitBindingDecl(const clang::BindingDecl* decl) {
        /// TODO:
        return true;
    }

    /// For `auto foo()|-> int| { return 1; }`
    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto loc = decl->getFunctionTypeLoc();
        if(!loc) {
            return true;
        }

        if(auto proto = llvm::dyn_cast<clang::FunctionProtoTypeLoc>(loc)){
            if(proto.getTypePtr()->hasTrailingReturn()){
                return true;
            }
        }

        auto returnLoc = loc.getReturnLoc();
        if(auto A = llvm::dyn_cast<clang::AutoTypeLoc>(returnLoc)) {
            using T = auto()->int;

            T x;
            return true;
        }

        /// TODO:
        return true;
    }

    /// For `foo(|x:|1,|y:|2)`
    bool VisitCallExpr(const clang::CallExpr* expr) {
        /// TODO:
        return true;
    }

    /// `Foo{|.x=|1,|.y=|2}`.
    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        /// TODO:
        return true;
    }

private:
    InlayHints result;
    index::Shared<InlayHints> sharedResult;
};

}  // namespace

}  // namespace clice::feature
