#include "Feature/InlayHint2.h"
#include "AST/FilterASTVisitor.h"

namespace clice::feature {

namespace {

class InlayHintsCollector : FilteredASTVisitor<InlayHintsCollector> {
public:
    using Base = FilteredASTVisitor<InlayHintsCollector>;
    using Base::Base;

public:
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

        if(auto proto = loc.getAs<clang::FunctionProtoTypeLoc>()) {
            if(proto.getTypePtr()->hasTrailingReturn()) {
                return true;
            }
        }

        auto returnLoc = loc.getReturnLoc();
        if(auto A = returnLoc.getAs<clang::AutoTypeLoc>()) {
            return true;
        }

        /// TODO:
        return true;
    }

    /// For `foo(|x:|1,|y:|2)`
    bool VisitCallExpr(const clang::CallExpr* expr) {
        /// For some builtin functions, skip them.
        switch(expr->getBuiltinCallee()) {
            case clang::Builtin::BIaddressof:
            case clang::Builtin::BIas_const:
            case clang::Builtin::BIforward:
            case clang::Builtin::BImove:
            case clang::Builtin::BImove_if_noexcept: {
                return true;
            }

            default: {
                break;
            }
        }

        auto callee = expr->getDirectCallee();

        /// Don't hint for UDL operator like `operaotr ""_str`.
        if(!callee || llvm::isa<clang::UserDefinedLiteral>(expr)) {
            return true;
        }

        /// Only hint for overloaded `operator()` and `operator[]`.
        if(auto OC = llvm::dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
            auto kind = OC->getOperator();
            if(kind != clang::OO_Call && kind != clang::OO_Subscript) {
                return true;
            }
        }

        /// Get all arguments of call expression.
        auto params = callee->parameters();
        if(callee->hasCXXExplicitFunctionObjectParameter()) {
            params = params.drop_front();
        }
        auto args = llvm::ArrayRef<const clang::Expr*>(expr->getArgs(), expr->getNumArgs());

        /// TODO: try to get the lparen location of call expr.
        tryHintArguments(clang::SourceLocation(), params, args);

        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* expr) {
        const auto* constructor = expr->getConstructor();
        if(!constructor) {
            return true;
        }

        auto params = constructor->parameters();
        auto args = clang::ArrayRef<const clang::Expr*>(expr->getArgs(), expr->getNumArgs());

        /// TODO: try to get the lparen location of call expr.
        tryHintArguments(clang::SourceLocation(), params, args);

        return true;
    }

    /// `Foo{|.x=|1,|.y=|2}`.
    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        /// TODO:
        return true;
    }

private:
    void addInlayHint(InlayHintKind kind, clang::SourceLocation location) {
        auto [fid, offset] = AST.getDecomposedLoc(location);
        auto& hints = interestedOnly ? result : sharedResult[fid];
    }

    void tryHintArguments(clang::SourceLocation lbrace,
                          llvm::ArrayRef<const clang::ParmVarDecl*> params,
                          llvm::ArrayRef<const clang::Expr*> args) {
        clang::SourceLocation lastArgLoc;
        for(std::size_t i = 0; i < args.size(); i++) {
            auto arg = args[i];
            auto loc = arg->getSourceRange().getBegin();

            auto& SM = AST.srcMgr();
            SM.isMacroArgExpansion(clang::SourceLocation());
            SM.isMacroBodyExpansion(clang::SourceLocation());

            if(lbrace == AST.getExpansionLoc(loc)) {
                /// If they have same location, they are both expansion location and expanded from
                /// macro.

                /// FIXME: Figure out how to filter this out.
                continue;
            } else {
                /// They have different location, try to hint the param.
                /// TODO: check param comment like `*/arg=/*`.
            }
        }
    }

private:
    InlayHints result;
    index::Shared<InlayHints> sharedResult;
};

}  // namespace

}  // namespace clice::feature
