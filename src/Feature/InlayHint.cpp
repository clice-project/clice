#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/InlayHint.h"

namespace clice {

namespace {

/// TODO:
/// Replace blank tooltip to something useful.

/// Create a blank markup content as a place holder.
proto::MarkupContent blank() {
    return {
        .value = "",
    };
}

/// Like clang::SourceRange but represents as a pair of offset (offset of the begin of main file).
struct OffsetRange {
    size_t begin;
    size_t end;

    /// Check if the range is not a `point` and end is after begin.
    bool isValid() {
        return end > begin;
    }
};

/// Compute inlay hints for a document in given range and config.
struct InlayHintCollector : clang::RecursiveASTVisitor<InlayHintCollector> {

    using Base = clang::RecursiveASTVisitor<InlayHintCollector>;

    const clang::SourceManager& src;

    const SourceConverter& cvtr;

    /// The config of inlay hints collector.
    const config::InlayHintOption config;

    /// The restrict range of request.
    const OffsetRange limit;

    /// The result of inlay hints.
    proto::InlayHintsResult result;

    /// Current file's uri.
    const proto::DocumentUri docuri;

    /// The printing policy of clang.
    const clang::PrintingPolicy policy;

    /// Whole source code text in main file.
    const llvm::StringRef code;

    /// Do not produce inlay hints if either range ends is not within the main file.
    bool needFilter(clang::SourceRange range) {
        // skip invalid range or not in main file
        if(range.isInvalid())
            return true;

        if(!src.isInMainFile(range.getBegin()) || !src.isInMainFile(range.getEnd()))
            return true;

        // not involved in restrict range
        auto begin = src.getDecomposedLoc(range.getBegin()).second;
        auto end = src.getDecomposedLoc(range.getEnd()).second;
        if(end < limit.begin || begin > limit.end)
            return true;

        return false;
    }

    /// Shrink the hint text to the max length.
    std::string shrinkText(std::string text) {
        if(text.size() > config.maxLength)
            text.resize(config.maxLength - 3), text.append("...");
        return text;
    }

    /// Collect hint for variable declared with `auto` keywords.
    /// The hint string wiil be placed at the right side of identifier, starting with ':' character.
    /// The `originDeclRange` will be used as the link of hint string.
    void collectAutoDeclHint(clang::QualType deduced, clang::SourceRange identRange,
                             std::optional<clang::SourceRange> linkDeclRange) {

        // For lambda expression, `getAsString` return a text like `(lambda at main.cpp:2:10)`
        //      auto lambda = [](){ return 1; };
        // Use a short text instead.
        std::string typeName = deduced.getAsString(policy);
        if(typeName.contains("lambda"))
            typeName = "(lambda)";

        proto::InlayHintLablePart lable{
            .value = shrinkText(std::format(": {}", typeName)),
            .tooltip = blank(),
        };

        if(linkDeclRange.has_value())
            lable.Location = {.uri = docuri, .range = cvtr.toRange(*linkDeclRange, src)};

        proto::InlayHint hint{
            .position = cvtr.toPosition(identRange.getEnd(), src),
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Type,
        };

        result.push_back(std::move(hint));
    }

    // If `expr` spells a single unqualified identifier, return that name, otherwise, return an
    // empty string.
    static llvm::StringRef takeExprIdentifier(const clang::Expr* expr) {
        auto spelled = expr->IgnoreUnlessSpelledInSource();
        if(auto* declRef = llvm::dyn_cast<clang::DeclRefExpr>(spelled))
            if(!declRef->getQualifier())
                return declRef->getDecl()->getName();
        if(auto* member = llvm::dyn_cast<clang::MemberExpr>(spelled))
            if(!member->getQualifier() && member->isImplicitAccess())
                return member->getMemberDecl()->getName();
        return {};
    }

    /// Check if there is any comment like /*paramName*/ before a argument.
    bool hasHandWriteComment(clang::SourceRange argument) {
        auto [fid, offset] = src.getDecomposedLoc(argument.getBegin());
        if(fid != src.getMainFileID())
            return false;

        // Get source text until the argument and drop end whitespace.
        llvm::StringRef content = code.substr(0, offset).rtrim();

        // Any comment ends with `*/` is considered meaningful.
        return content.ends_with("*/");
    }

    bool needHintArgument(const clang::ParmVarDecl* param, const clang::Expr* arg) {
        auto name = param->getName();

        // Skip anonymous parameters.
        if(name.empty())
            return false;

        // Skip if the argument is a single name and it matches the parameter exactly.
        if(name.equals_insensitive(takeExprIdentifier(arg)))
            return false;

        // Skip if the argument is preceded by any hand-written hint /*paramName*/.
        if(hasHandWriteComment(arg->getSourceRange()))
            return false;

        return true;
    }

    bool isPassedAsMutableLValueRef(const clang::ParmVarDecl* param) {
        auto qual = param->getType();
        return qual->isLValueReferenceType() && !qual.getNonReferenceType().isConstQualified();
    }

    void collectArgumentHint(llvm::ArrayRef<const clang::ParmVarDecl*> params,
                             llvm::ArrayRef<const clang::Expr*> args) {
        for(size_t i = 0; i < params.size() && i < args.size(); ++i) {
            // Pack expansion and default argument is always the tail of arguments.
            if(llvm::isa<clang::PackExpansionExpr>(args[i]) ||
               llvm::isa<clang::CXXDefaultArgExpr>(args[i]))
                break;

            if(!needHintArgument(params[i], args[i]))
                continue;

            // Only hint reference for mutable lvalue reference.
            const bool hintRef = isPassedAsMutableLValueRef(params[i]);
            proto::InlayHintLablePart lable{
                .value = shrinkText(std::format("{}{}:", params[i]->getName(), hintRef ? "&" : "")),
                .tooltip = blank(),
                .Location =
                    proto::Location{.uri = docuri,
                                    .range = cvtr.toRange(params[i]->getSourceRange(), src)}
            };

            proto::InlayHint hint{
                .position = cvtr.toPosition(args[i]->getSourceRange().getBegin(), src),
                .lable = {std::move(lable)},
                .kind = proto::InlayHintKind::Parameter,
            };

            result.push_back(std::move(hint));
        }
    }

    bool TraverseDecl(clang::Decl* decl) {
        if(!decl || needFilter(decl->getSourceRange()))
            return true;

        return Base::TraverseDecl(decl);
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        // Hint local variable, global variable, and structure binding.
        if(!decl->isLocalVarDecl() && !decl->isFileVarDecl())
            return true;

        if(!config.dedcucedType)
            return true;

        // Hint for indivadual element of structure binding.
        if(auto bind = llvm::dyn_cast<clang::DecompositionDecl>(decl)) {
            for(auto* binding: bind->bindings()) {
                // Hint for used variable only.
                if(auto type = binding->getType(); !type.isNull() && !type->isDependentType()) {
                    // Hint at the end position of identifier.
                    auto name = binding->getName();
                    collectAutoDeclHint(type.getCanonicalType(),
                                        binding->getBeginLoc().getLocWithOffset(name.size()),
                                        decl->getSourceRange());
                }
            }
            return true;
        }

        /// skip dependent type.
        clang::QualType qty = decl->getType();
        if(qty.isNull() || qty->isDependentType())
            return true;

        if(const auto at = qty->getContainedAutoType()) {
            // Use most recent decl as the link of hint string.
            /// FIXME:
            /// Shall we use the first decl as the link of hint string?
            std::optional<clang::SourceRange> originDeclRange;
            if(const auto mrd = decl->getMostRecentDecl())
                originDeclRange = mrd->getSourceRange();

            auto tailOfIdentifier = decl->getLocation().getLocWithOffset(decl->getName().size());
            collectAutoDeclHint(qty, tailOfIdentifier, originDeclRange);
        }
        return true;
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

    /// Try find the FunctionProtoType of a CallExpr which callee is a function pointer.
    static auto detectCallViaFnPointer(const clang::Expr* call)
        -> std::optional<clang::FunctionProtoTypeLoc> {

        auto nake = call->IgnoreParenCasts();
        clang::TypeLoc target;

        if(auto* tydef = nake->getType().getTypePtr()->getAs<clang::TypedefType>())
            target = tydef->getDecl()->getTypeSourceInfo()->getTypeLoc();
        else if(auto* declRef = llvm::dyn_cast<clang::DeclRefExpr>(nake))
            if(auto* varDecl = llvm::dyn_cast<clang::VarDecl>(declRef->getDecl()))
                target = varDecl->getTypeSourceInfo()->getTypeLoc();

        if(!target)
            return std::nullopt;

        // Unwrap types that may be wrapping the function type.
        while(true) {
            if(auto P = target.getAs<clang::PointerTypeLoc>())
                target = P.getPointeeLoc();
            else if(auto A = target.getAs<clang::AttributedTypeLoc>())
                target = A.getModifiedLoc();
            else if(auto P = target.getAs<clang::ParenTypeLoc>())
                target = P.getInnerLoc();
            else
                break;
        }

        if(auto proto = target.getAs<clang::FunctionProtoTypeLoc>())
            return proto;

        return std::nullopt;
    }

    bool VisitCallExpr(const clang::CallExpr* call) {
        if(!config.argumentName)
            return true;

        // Don't hint for UDL operator like `operaotr ""_str` , and builtin funtion.
        if(!call || llvm::isa<clang::UserDefinedLiteral>(call) || isBuiltinFnCall(call))
            return true;

        // They were handled in  `VisitCXXMemberCallExpr`, `VisitCXXOperatorCallExpr`.
        if(llvm::isa<clang::CXXMemberCallExpr>(call) || llvm::isa<clang::CXXOperatorCallExpr>(call))
            return true;

        // For a CallExpr, there are 2 case of Callee:
        //     1. An object which has coresponding FunctionDecl, free function or method.
        //     2. A function pointer, which has no FunctionDecl but FunctionProtoTypeLoc.

        // Use FunctionDecl if callee is a free function or method.
        const clang::FunctionDecl* fndecl = nullptr;
        const clang::Decl* calleeDecl = call->getCalleeDecl();
        if(auto decl = llvm::dyn_cast<clang::FunctionDecl>(calleeDecl))
            fndecl = decl;
        else if(auto tfndecl = llvm::dyn_cast<clang::FunctionTemplateDecl>(calleeDecl))
            fndecl = tfndecl->getTemplatedDecl();

        if(fndecl)
            // free function
            collectArgumentHint(fndecl->parameters(), {call->getArgs(), call->getNumArgs()});
        else if(auto proto = detectCallViaFnPointer(call->getCallee()); proto.has_value())
            // function pointer
            collectArgumentHint(proto->getParams(), {call->getArgs(), call->getNumArgs()});

        return true;
    }

    bool VisitCXXOperatorCallExpr(const clang::CXXOperatorCallExpr* call) {
        if(!config.argumentName)
            return true;

        // Do not hint paramters for operator overload except `operator()`, and `operator[]` with
        // only one parameter.
        auto opkind = call->getOperator();
        if(opkind == clang::OO_Call || opkind == clang::OO_Subscript && call->getNumArgs() != 1) {
            auto method = llvm::dyn_cast<clang::CXXMethodDecl>(call->getCalleeDecl());

            llvm::ArrayRef<const clang::ParmVarDecl*> params{method->parameters()};
            llvm::ArrayRef<const clang::Expr*> args{call->getArgs(), call->getNumArgs()};

            // Skip `this` parameter declaration if callee is CXXMethodDecl.
            if(!method->hasCXXExplicitFunctionObjectParameter())
                args = args.drop_front();

            collectArgumentHint(params, args);
        }

        return true;
    }

    static bool isSimpleSetter(const clang::CXXMethodDecl* md) {
        if(md->getNumParams() != 1)
            return false;

        auto name = md->getName();
        if(!name.starts_with_insensitive("set"))
            return false;

        // Check that the part after "set" matches the name of the parameter (ignoring case). The
        // idea here is that if the parameter name differs, it may contain extra information that
        // may be useful to show in a hint, as in:
        //   void setTimeout(int timeoutMillis);
        // The underscores in FunctionName and Parameter will be ignored.
        llvm::SmallString<32> param, fnname;
        for(auto c: name.drop_front(3))
            if(c != '_')
                fnname.push_back(c);

        for(auto c: md->getParamDecl(0)->getName())
            if(c != '_')
                param.push_back(c);

        return fnname.equals_insensitive(param);
    }

    bool VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr* call) {
        if(!config.argumentName)
            return true;

        auto callee = llvm::dyn_cast<clang::FunctionDecl>(call->getCalleeDecl());

        // Do not hint move / copy constructor call.
        if(auto ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(callee))
            if(ctor->isCopyOrMoveConstructor())
                return true;

        // Do not hint simple setter function call. e.g. `setX(1)`.
        if(auto md = llvm::dyn_cast<clang::CXXMethodDecl>(callee))
            if(isSimpleSetter(md))
                return true;

        llvm::ArrayRef<const clang::ParmVarDecl*> params{callee->parameters()};
        llvm::ArrayRef<const clang::Expr*> args{call->getArgs(), call->getNumArgs()};

        // Skip `this` parameter declaration if callee is CXXMethodDecl.
        if(auto md = llvm::dyn_cast<clang::CXXMethodDecl>(callee))
            if(md->hasCXXExplicitFunctionObjectParameter())
                args = args.drop_front();

        collectArgumentHint(params, args);
        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* ctor) {
        if(!config.argumentName)
            return true;

        // Skip constructor call without an argument list, by checking the validity of
        // getParenOrBraceRange(). Also skip std::initializer_list constructors.
        if(!ctor->getParenOrBraceRange().isValid() || ctor->isStdInitListInitialization())
            return true;

        if(const auto decl = ctor->getConstructor())
            collectArgumentHint(decl->parameters(), {ctor->getArgs(), ctor->getNumArgs()});

        return true;
    }

    void collectReturnTypeHint(clang::SourceLocation hintLoc, clang::QualType retType,
                               clang::SourceRange retTypeDeclRange) {
        proto::InlayHintLablePart lable{
            .value = shrinkText(std::format("-> {}", retType.getAsString(policy))),
            .tooltip = blank(),
            .Location =
                proto::Location{.uri = docuri, .range = cvtr.toRange(retTypeDeclRange, src)}
        };

        proto::InlayHint hint{
            .position = cvtr.toPosition(hintLoc, src),
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Type,
        };

        result.push_back(std::move(hint));
    }

    // bool TraverseFunctionDecl(clang::FunctionDecl* decl) {
    //     return config.returnType ? Base::TraverseFunctionDecl(decl) : true;
    // }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        // 1. Hint block end.
        if(config.blockEnd && decl->isThisDeclarationADefinition()) {
            /// FIXME:
            /// Use a proper name such as simplified signature of funtion.
            auto typeLoc = decl->getTypeSourceInfo()->getTypeLoc().getSourceRange();
            auto begin = src.getCharacterData(typeLoc.getBegin());
            auto end = src.getCharacterData(typeLoc.getEnd());
            llvm::StringRef piece{begin, static_cast<size_t>(end - begin) + 1};

            //
            collectBlockEndHint(decl->getBodyRBrace().getLocWithOffset(1),
                                std::format("// {}", piece),
                                decl->getSourceRange(),
                                DecideDuplicated::Ignore);
        }

        // 2. Hint return type.
        if(!config.returnType)
            return true;

        if(auto proto = llvm::dyn_cast<clang::FunctionProtoType>(decl->getType().getTypePtr()))
            if(proto->hasTrailingReturn())
                return true;

        if(auto fnTypeLoc = decl->getFunctionTypeLoc())
            // Hint for function declaration with `auto` or `decltype(...)` return type.
            if(fnTypeLoc.getReturnLoc().getContainedAutoTypeLoc())
                // Right side of ')' in parameter list.
                collectReturnTypeHint(fnTypeLoc.getRParenLoc().getLocWithOffset(1),
                                      decl->getReturnType(),
                                      decl->getSourceRange());

        return true;
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* expr) {
        // 1. Hint block end.
        if(config.blockEnd)
            collectBlockEndHint(
                expr->getEndLoc().getLocWithOffset(1),
                std::format("// lambda #{}", expr->getLambdaClass()->getLambdaManglingNumber()),
                expr->getSourceRange(),
                DecideDuplicated::Replace);

        // 2. Hint return type.
        if(!config.returnType)
            return true;

        clang::FunctionDecl* decl = expr->getCallOperator();
        if(expr->hasExplicitResultType())
            return true;

        // where to place the hint position, in default it is an invalid value.
        clang::SourceLocation hintLoc = {};
        if(!expr->hasExplicitParameters())
            // right side of ']' after the capture list.
            hintLoc = expr->getIntroducerRange().getEnd().getLocWithOffset(1);
        else if(auto fnTypeLoc = decl->getFunctionTypeLoc())
            // right side of ')'.
            hintLoc = fnTypeLoc.getRParenLoc().getLocWithOffset(1);

        if(hintLoc.isValid())
            collectReturnTypeHint(hintLoc, decl->getReturnType(), decl->getSourceRange());

        return true;
    }

    void collectArrayElemIndexHint(int index, clang::SourceLocation location) {
        proto::InlayHintLablePart lable{
            .value = std::format("[{}]=", index),  // This shouldn't be shrinked.
            .tooltip = blank(),
        };

        proto::InlayHint hint{
            .position = cvtr.toPosition(location, src),
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Parameter,
        };

        result.push_back(std::move(hint));
    }

    bool VisitInitListExpr(const clang::InitListExpr* Syn) {
        int count = 0;
        for(auto init: Syn->inits()) {
            if(llvm::isa<clang::DesignatedInitExpr>(init) ||
               hasHandWriteComment(init->getSourceRange()))
                continue;

            // Only hint for the first config.maxArrayElements elements.
            if(count++ >= config.maxArrayElements)
                break;

            collectArrayElemIndexHint(count, init->getBeginLoc());
        }
        return true;
    }

    bool isMultiLineRange(const clang::SourceRange range) {
        return range.isValid() && src.getPresumedLineNumber(range.getBegin()) <
                                      src.getPresumedLineNumber(range.getEnd());
    }

    llvm::StringRef remainTextOfThatLine(clang::SourceLocation location) {
        auto [_, offset] = src.getDecomposedLoc(location);
        auto remain = code.substr(offset).split('\n').first;
        return remain.ltrim();
    }

    /// This enum decide how to handle the duplicated hint in the same line.
    enum class DecideDuplicated {
        // Accept all hints.
        AcceptBoth,

        // Drop the old hint, and accept the new hint. Commonly use the inner one.
        //      namespace out::in {
        //      } |// namespace in|
        Replace,

        // Ignore the new hint, and keep the old hint. Commonly use the outer one.
        //      struct Out {
        //          struct In {
        //      }} |// struct Out|;
        Ignore,
    };

    void collectBlockEndHint(clang::SourceLocation location, std::string text,
                             clang::SourceRange linkRange, DecideDuplicated decision) {
        // Already has a comment in that line.
        if(auto remain = remainTextOfThatLine(location);
           remain.starts_with("/*") || remain.starts_with("//"))
            return;

        // Already has a duplicated hint in that location, use the newer hint instead.
        const auto lspPosition = cvtr.toPosition(location, src);
        if(decision != DecideDuplicated::AcceptBoth && !result.empty())
            if(const auto& last = result.back().position; last.line == lspPosition.line) {
                if(decision == DecideDuplicated::Replace)
                    result.pop_back();
                else
                    return;
            }

        proto::InlayHintLablePart lable{
            .value = shrinkText(std::move(text)),
            .tooltip = blank(),
            .Location = {.uri = docuri, .range = cvtr.toRange(linkRange, src)},
        };

        proto::InlayHint hint{
            .position = lspPosition,
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Parameter,
        };

        result.push_back(std::move(hint));
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        if(!config.blockEnd)
            return true;

        auto range = decl->getSourceRange();
        if(decl->isAnonymousNamespace() || !isMultiLineRange(range))
            return true;

        collectBlockEndHint(decl->getRBraceLoc().getLocWithOffset(1),
                            std::format("// namespace {}", decl->getName()),
                            range,
                            DecideDuplicated::Replace);
        return true;
    }

    void collectStructSizeAndAlign(const clang::TagDecl* decl) {
        if(!decl->isStruct() && !decl->isClass())
            return;

        auto& ctx = decl->getASTContext();
        auto qual = decl->getTypeForDecl()->getCanonicalTypeInternal();

        auto size = ctx.getTypeSizeInChars(qual).getQuantity();
        auto align = ctx.getTypeAlignInChars(qual).getQuantity();

        proto::InlayHintLablePart lable{
            .value = shrinkText(std::format("size: {}, align: {}", size, align)),
            .tooltip = blank(),
            .Location = {.uri = docuri, .range = cvtr.toRange(decl->getSourceRange(), src)},
        };

        // right side of identifier.
        auto tail = decl->getLocation().getLocWithOffset(decl->getName().size());
        proto::InlayHint hint{
            .position = cvtr.toPosition(tail, src),
            .lable = {std::move(lable)},
            .kind = proto::InlayHintKind::Parameter,
        };

        result.push_back(std::move(hint));
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        if(!decl->isThisDeclarationADefinition())
            return true;

        if(config.blockEnd) {
            std::string hintText = std::format("// {}", decl->getKindName().str());
            // Add a tail flag for enum declaration as clangd's do.
            if(const auto* enumDecl = llvm::dyn_cast<clang::EnumDecl>(decl);
               enumDecl && enumDecl->isScoped())
                hintText += enumDecl->isScopedUsingClassTag() ? " class" : " struct";

            // Format text to 'struct Example' or `class Example` or `enum class Example`
            hintText.append(" ").append(decl->getName());
            collectBlockEndHint(decl->getBraceRange().getEnd().getLocWithOffset(1),
                                std::move(hintText),
                                decl->getSourceRange(),
                                DecideDuplicated::Ignore);
        }

        if(config.structSizeAndAlign)
            collectStructSizeAndAlign(decl);

        return true;
    }

    /// TODO:
    /// Find proper end location of cast expression.
    // bool VisitImplicitCastExpr(const clang::ImplicitCastExpr* stmt) {
    //     if(!config.implicitCast)
    //         return true;
    //     if(auto* expr = llvm::dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    //         proto::InlayHintLablePart lable{
    //             .value = shrinkText(std::format("as {}", expr->getType().getAsString(policy))),
    //             .tooltip = blank(),
    //         };
    //         // right side of that expr.
    //         proto::InlayHint hint{
    //             .position = cvtr.toPosition(stmt->getEndLoc()),
    //             .lable = {std::move(lable)},
    //             .kind = proto::InlayHintKind::Parameter,
    //         };
    //         result.push_back(std::move(hint));
    //     }
    //     return true;
    // }
};

}  // namespace

namespace feature {

json::Value inlayHintCapability(json::Value InlayHintClientCapabilities) {
    return {};
}

/// Compute inlay hints for a document in given range and config.
proto::InlayHintsResult inlayHints(proto::InlayHintParams param, ASTInfo& info,
                                   const SourceConverter& converter,
                                   const config::InlayHintOption& config) {
    const clang::SourceManager& src = info.srcMgr();

    llvm::StringRef codeText = src.getBufferData(src.getMainFileID());

    // Take 0-0 based Lsp Location from `param.range` and convert it to offset pair.
    OffsetRange requestRange{
        .begin = converter.toOffset(codeText, param.range.start),
        .end = converter.toOffset(codeText, param.range.end),
    };

    // In default, use the whole main file as the restrict range.
    if(!requestRange.isValid()) {
        clang::FileID main = src.getMainFileID();
        requestRange.begin = src.getDecomposedSpellingLoc(src.getLocForStartOfFile(main)).second;
        requestRange.end = src.getDecomposedSpellingLoc(src.getLocForEndOfFile(main)).second;
    }

    /// TODO:
    /// Check and fix invalid options before collect hints.
    InlayHintCollector collector{
        .src = src,
        .cvtr = converter,
        .config = config,
        .limit = requestRange,
        .docuri = std::move(param.textDocument.uri),
        .policy = info.context().getPrintingPolicy(),
        .code = codeText,
    };

    collector.TraverseTranslationUnitDecl(info.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
