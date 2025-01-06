#include "Feature/InlayHint.h"

namespace clice {

namespace {

struct LspProtoAdaptor {

    const clang::SourceManager* src;

    bool isInMainFile(clang::SourceLocation loc) {
        return loc.isValid() && src->isInMainFile(loc);
    }

    bool notInMainFile(clang::SourceLocation loc) {
        return !isInMainFile(loc);
    }

    proto::Position toLspPosition(clang::SourceLocation loc) {
        auto presumed = src->getPresumedLoc(loc);
        return {
            .line = presumed.getLine() - 1,
            .character = presumed.getColumn() - 1,
        };
    }

    proto::Range toLspRange(clang::SourceRange sr) {
        return {
            .start = toLspPosition(sr.getBegin()),
            .end = toLspPosition(sr.getEnd()),
        };
    }
};

/// TODO:
/// Replace blank tooltip to something useful.

/// Create a blank markup content as a place holder.
proto::MarkupContent blank() {
    return {
        .kind = proto::MarkupKind::PlainText,
        .value = "",
    };
}

/// Compute inlay hints for a document in given range and config.
struct InlayHintCollector : clang::RecursiveASTVisitor<InlayHintCollector>, LspProtoAdaptor {

    using Base = clang::RecursiveASTVisitor<InlayHintCollector>;

    /// The config of inlay hints collector.
    const config::InlayHintConfig& config;

    /// The restrict range of request.
    clang::SourceRange limit;

    /// The result of inlay hints.
    proto::InlayHintsResult result;

    /// Current file's uri.
    proto::DocumentUri docuri;

    /// The printing policy of clang.
    clang::PrintingPolicy policy;

    /// Do not produce inlay hints if either range ends is not within the main file.
    bool needFilter(clang::SourceRange range) {
        // skip invalid range or not in main file
        if(range.isInvalid())
            return true;

        if(!src->isInMainFile(range.getBegin()) || !src->isInMainFile(range.getEnd()))
            return true;

        // not involved in restrict range
        if(range.getEnd() < limit.getBegin() || range.getBegin() > limit.getEnd())
            return true;

        return false;
    }

    /// Collect hint for variable declared with `auto` keywords.
    /// The hint string wiil be placed at the right side of identifier, starting with ':' character.
    /// The `originDeclRange` will be used as the link of hint string.
    void collectAutoDeclHint(clang::QualType deduced, clang::SourceRange identRange,
                             std::optional<clang::SourceRange> linkDeclRange) {
        proto::InlayHintLablePart lable{
            .value = std::format(": {}", deduced.getAsString(policy)),
            .tooltip = blank(),
        };

        if(linkDeclRange.has_value())
            lable.Location = {.uri = docuri, .range = toLspRange(*linkDeclRange)};

        proto::InlayHint hint{
            .position = toLspPosition(identRange.getEnd()),
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Type,
        };

        result.push_back(std::move(hint));
    }

    void collectArgNameHint() {}

    bool TraverseDecl(clang::Decl* decl) {
        if(!decl || needFilter(decl->getSourceRange()))
            return true;

        return Base::TraverseDecl(decl);
    }

    bool TraverseVarDecl(clang::VarDecl* decl) {
        clang::QualType qty = decl->getType();

        /// skip dependent type.
        if(qty.isNull() || qty->isDependentType())
            return true;

        if(auto at = qty->getContainedAutoType()) {
            // Use most recent decl as the link of hint string.
            /// FIXME:
            /// Shall we use the first decl as the link of hint string?
            std::optional<clang::SourceRange> originDeclRange;
            if(auto mrd = decl->getMostRecentDecl())
                originDeclRange = mrd->getSourceRange();

            auto tailOfIdentifier = decl->getLocation().getLocWithOffset(decl->getName().size());
            collectAutoDeclHint(qty, tailOfIdentifier, originDeclRange);
            return true;
        }

        return Base::TraverseVarDecl(decl);
    }

    static bool isBuiltinFnCall(const clang::CallExpr* expr) {
        namespace btin = clang::Builtin;
        switch(expr->getBuiltinCallee()) {
            case btin::BIaddressof:
            case btin::BIas_const:
            case btin::BIforward:
            case btin::BImove:
            case btin::BImove_if_noexcept: return true;
            default: return false;
        }
    }

    bool TraverseCallExpr(clang::CallExpr* expr) {
        // Do not show hint for user defined literals operator like ` operaotr ""_str , and builtin
        // funtion call.
        if(!expr || llvm::isa<clang::UserDefinedLiteral>(expr) || isBuiltinFnCall(expr))
            return true;

        if(auto mc = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr))
            return TraverseCXXMemberCallExpr(mc);

        return Base::TraverseCallExpr(expr);
    }

    bool TraverseCXXMemberCallExpr(clang::CXXMemberCallExpr* expr) {
        // Do not hint paramters for operator overload except ` operator() `.
        if(auto opcall = llvm::dyn_cast<clang::CXXOperatorCallExpr>(expr))
            if(opcall->getOperator() != clang::OO_Call)
                return true;

        // Do not hint move / copy constructor call.
        if(auto ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(expr->getCalleeDecl()))
            if(ctor->isCopyOrMoveConstructor())
                return true;

        /// TODO:
        /// Do not hint for simple setter function call. e.g. `setX(1)`.
        // if (isSimpleSetter(expr->getCalleeDecl()->getAsFunction()->getNameAsString()))
        //     ...

        llvm::ArrayRef<const clang::Expr*> args{expr->getArgs(), expr->getNumArgs()};

        // Skip `this` parameter declaration.
        if(auto md = llvm::dyn_cast<clang::CXXMethodDecl>(expr->getCalleeDecl()))
            if(md->hasCXXExplicitFunctionObjectParameter())
                args = args.drop_front();

        return Base::TraverseCXXMemberCallExpr(expr);
    }

    bool VisitFunctionDecl(clang::FunctionDecl* decl) {
        /// TODO:
        /// Hint return type for function declaration.
        return Base::VisitFunctionDecl(decl);
    }
};

}  // namespace

namespace feature {

json::Value inlayHintCapability(json::Value InlayHintClientCapabilities) {
    return {};
}

/// Compute inlay hints for a document in given range and config.
proto::InlayHintsResult inlayHints(proto::InlayHintParams param, ASTInfo& ast,
                                   const config::InlayHintConfig& config) {
    clang::SourceManager* src = &ast.srcMgr();

    /// FIXME:
    /// Take 0-0 based Lsp Location from `param.range` and convert it to clang 1-1 based
    /// source location.
    clang::SourceRange fixedRange;  // = range...

    // In default, use the whole main file as the restrict range.
    if(fixedRange.isInvalid()) {
        clang::FileID main = src->getMainFileID();
        fixedRange = {src->getLocForStartOfFile(main), src->getLocForEndOfFile(main)};
    }

    InlayHintCollector collector{
        .config = config,
        .limit = fixedRange,
        .docuri = std::move(param.textDocument.uri),
        .policy = ast.context().getPrintingPolicy(),
    };
    collector.src = src;

    collector.TraverseTranslationUnitDecl(ast.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
