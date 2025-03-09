#include "AST/FilterASTVisitor.h"
#include "AST/Utility.h"
#include "Compiler/Compilation.h"
#include "Feature/InlayHint.h"
#include "Server/LSPConverter.h"

#include "clang/AST/TypeVisitor.h"

namespace clice::feature {

namespace {

/// For a given `clang::QualType`, build a list of `LablePart` and make each part clickable.
/// e.g, for hints like:
///     auto var|: MyNamespace::MyClass| = some_expr;
/// the text `MyNamespace` and `MyClass` will be linked to its declaration.
/// While for type in std namespace and built-in types like:
///     auto var|: std::vector<int>| = another_expr;
/// the type `std::vector<int>` will not link to anything, it's just a text.
struct TypeHintLinkBuilder : clang::TypeVisitor<TypeHintLinkBuilder, void> {
    using Base = clang::TypeVisitor<TypeHintLinkBuilder, void>;

    ASTInfo& AST;

    // The result buffer to write.
    std::vector<LabelPart>& results;

    void VisitPointerType(const clang::PointerType* T) {
        Visit(T->getPointeeType().getTypePtr());
    }

    void VisitReferenceType(const clang::ReferenceType* T) {
        Visit(T->getPointeeType().getTypePtr());
    }

    void VisitElaboratedType(const clang::ElaboratedType* T) {
        Visit(T->getNamedType().getTypePtr());
    }

    void VisitTypedefType(const clang::TypedefType* T) {
        Visit(T->desugar().getTypePtr());
    }

    void VisitDeducedType(const clang::DeducedType* T) {
        Visit(T->getDeducedType().getTypePtr());
    }

    template <typename Decl>
    void recordScope(const Decl* D, llvm::SmallVectorImpl<LabelPart>& stack) {
        stack.push_back({
            .value = getDeclName(D),
            .link = AST.toLocalRange(D->getSourceRange()).second,
        });
    }

    // Recursively record namespace for custom type.
    void recursiveMarkScope(const clang::DeclContext* DC) {
        // for a type like `NamespaceA::NamespaceB::MyStruct`, we traverse the DeclContext from
        // inner to outer to find all namespaces, in the order like `NamespaceB -> NamespaceA`. So
        // use a stack to store the result
        llvm::SmallVector<LabelPart, 3> stack;

        while(DC) {
            if(const auto* ND = llvm::dyn_cast<clang::NamespaceDecl>(DC)) {
                recordScope(ND, stack);
            } else if(const auto* RD = llvm::dyn_cast<clang::RecordDecl>(DC)) {
                recordScope(RD, stack);
            }
            DC = DC->getParent();
        }

        while(!stack.empty()) {
            results.push_back(stack.pop_back_val());
            results.push_back({.value = "::"});
        }
    }

    void VisitTagType(const clang::TagType* T) {
        const clang::TagDecl* RD = T->getDecl();
        recursiveMarkScope(RD->getDeclContext());
        results.push_back({
            .value = RD->getName().str(),
            .link = AST.toLocalRange(RD->getFirstDecl()->getSourceRange()).second,
        });
    }

    void VisitTemplateSpecializationType(const clang::TemplateSpecializationType* T) {
        // Recursively process template arguments
        for(const clang::TemplateArgument& Arg: T->template_arguments()) {
            if(Arg.getKind() == clang::TemplateArgument::Type) {
                Visit(Arg.getAsType().getTypePtr());
            }
        }
    }

    static bool isBuiltinType(const clang::QualType& QT) {
        return QT->isBuiltinType() ||
               QT.getNonReferenceType()->getUnqualifiedDesugaredType()->isBuiltinType();
    }

    // Recursively check if the given DeclContext is in std namespace.
    static bool isInStdNamespace(const clang::DeclContext* DC) {
        while(DC) {
            if(const auto* ND = llvm::dyn_cast<clang::NamespaceDecl>(DC)) {
                if(ND->getName() == "std")
                    return true;
            }
            DC = DC->getParent();  // Continue searching upwards
        }
        return false;
    }

    static bool isStdType(const clang::QualType& QT) {
        const clang::Type* T = QT.getTypePtrOrNull();
        if(!T)
            return false;

        // ElaboratedType like `class std::vector<int>`
        if(const auto* ET = llvm::dyn_cast<clang::ElaboratedType>(T)) {
            return isStdType(ET->getNamedType());
        }

        // TypedefType like `std::string`
        if(const auto* TT = llvm::dyn_cast<clang::TypedefType>(T)) {
            return isInStdNamespace(TT->getDecl()->getDeclContext());
        }

        // RecordType like `std::vector<int>`
        if(const auto* RT = llvm::dyn_cast<clang::RecordType>(T)) {
            return isInStdNamespace(RT->getDecl()->getDeclContext());
        }

        // TemplateSpecializationType like `std::vector<int>`
        if(const auto* TST = llvm::dyn_cast<clang::TemplateSpecializationType>(T)) {
            if(const auto* TD = TST->getTemplateName().getAsTemplateDecl()) {
                return isInStdNamespace(TD->getDeclContext());
            }
        }

        // InjectedClassNameType like `std::vector<int>`
        if(const auto* ICT = llvm::dyn_cast<clang::InjectedClassNameType>(T)) {
            return isInStdNamespace(ICT->getDecl()->getDeclContext());
        }

        // PointerType like `std::string*`
        if(const auto* PT = llvm::dyn_cast<clang::PointerType>(T)) {
            return isStdType(PT->getPointeeType());
        }

        // ReferenceType like std::string&
        if(const auto* RT = llvm::dyn_cast<clang::ReferenceType>(T)) {
            return isStdType(RT->getPointeeType());
        }

        // DependentNameType like `std::vector<T>`
        if(const auto* DNT = llvm::dyn_cast<clang::DependentNameType>(T)) {
            /// FIXME: How to check std namespace for DependentNameType ?
            return false;
        }

        return false;
    }

    /// Build label parts for a given type, the result will be written to `hints`.
    static void build(clang::QualType QT, std::vector<LabelPart>& hints, ASTInfo& AST) {
        assert(!QT.isNull() && "QualType must not be Null.");

        if(isBuiltinType(QT) || isStdType(QT)) {
            hints.push_back({.value = QT.getAsString()});
            return;
        }

        TypeHintLinkBuilder builder{.AST = AST, .results = hints};
        builder.Visit(QT.getTypePtr());
    }

    /// Same with `build`, but we have a known name of the type from `QT.getAsString()`.
    static void buildWithKnownName(clang::QualType QT,
                                   llvm::StringRef name,
                                   std::vector<LabelPart>& hints,
                                   ASTInfo& AST) {
        assert(!QT.isNull() && "QualType must not be Null.");

        if(name.contains("std::") || isBuiltinType(QT)) {
            hints.push_back({.value = name.str()});
            return;
        }

        TypeHintLinkBuilder builder{.AST = AST, .results = hints};
        builder.Visit(QT.getTypePtr());
    }
};

/// Compute inlay hints for a AST. There is two kind of collection:
///     A. Only collect hints in MainFileID.
///     B. Collect hints in each files, used for header context.
/// The result is always stored in a densmap<FileID, vector<InlayHint>>, and return as needed.
struct InlayHintCollector : public FilteredASTVisitor<InlayHintCollector> {

    using Base = FilteredASTVisitor<InlayHintCollector>;

    using Kind = InlayHintKind;

    /// The result of inlay hints for given AST.
    using Storage = llvm::DenseMap<clang::FileID, std::vector<InlayHint>>;

    /// The result of inlay hints.
    Storage result;

    /// The config of inlay hints collector.
    const config::InlayHintOption option;

    /// Whole source code text in main file.
    const llvm::StringRef code;

    InlayHintCollector(ASTInfo& ast,
                       bool interestedOnly,
                       std::optional<LocalSourceRange> limit,
                       const config::InlayHintOption& option) :
        Base(ast, interestedOnly, limit), result(), option(option),
        code(ast.getInterestedFileContent()) {}

    /// Shrink the hint text to the max length.
    static std::string shrinkHintText(std::string text, size_t maxLength) {
        if(maxLength > 3)
            text.resize(maxLength - 3), text.append("...");
        else
            text.resize(maxLength);

        text.shrink_to_fit();
        return text;
    }

    std::string tryShrinkHintText(std::string text) {
        // Do not shrink hint text for index, or the max length is unlimited.
        if(!interestedOnly || option.maxLength == 0) {
            return text;
        }

        if(auto max = option.maxLength; text.size() > option.maxLength) {
            if(max > 3) {
                text.resize(max - 3), text.append("...");
            } else {
                text.resize(max);
            }
            text.shrink_to_fit();
        }
        return text;
    }

    /// Collect hint for variable declared with `auto` keywords.
    /// The hint string wiil be placed at the right side of identifier, starting with ':' character.
    /// The `originDeclRange` will be used as the link of hint string.
    void collectAutoDeclTypeHint(clang::QualType deduced,
                                 clang::SourceRange identRange,
                                 std::optional<clang::SourceRange> linkDeclRange,
                                 Kind kind) {

        // For lambda expression, `getAsString` return a text like `(lambda at main.cpp:2:10)`
        //      auto lambda = [](){ return 1; };
        // Use a short text instead.
        std::string typeName = deduced.getAsString(AST.context().getPrintingPolicy());

        bool isLambda = false;
        if(typeName.contains("lambda"))
            typeName = "(lambda)", isLambda = true;

        std::vector<LabelPart> labels;
        if(isLambda || !option.typeLink) {
            LabelPart lable{.value = tryShrinkHintText(std::format(": {}", typeName))};
            if(linkDeclRange.has_value())
                lable.link = AST.toLocalRange(*linkDeclRange).second;
            labels.push_back(std::move(lable));
        } else {
            labels.push_back({.value = ": "});
            TypeHintLinkBuilder::buildWithKnownName(deduced, typeName, labels, AST);
        }

        auto [locFileID, offset] = AST.getDecomposedLoc(identRange.getEnd());
        InlayHint hint{
            .kind = kind,
            .offset = offset,
            .labels = std::move(labels),
        };
        clang::FileID fileID = interestedOnly ? AST.getInterestedFile() : locFileID;
        result[fileID].push_back(std::move(hint));
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
    bool hasHandWriteComment(clang::SourceRange argumentRange) {
        auto [fileID, offset] = AST.getDecomposedLoc(argumentRange.getBegin());
        if(fileID != AST.getInterestedFile())
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
                             llvm::ArrayRef<const clang::Expr*> args,
                             Kind kind) {
        for(size_t i = 0; i < params.size() && i < args.size(); ++i) {
            // Pack expansion and default argument is always the tail of arguments.
            if(llvm::isa<clang::PackExpansionExpr>(args[i]) ||
               llvm::isa<clang::CXXDefaultArgExpr>(args[i]))
                break;

            if(!needHintArgument(params[i], args[i]))
                continue;

            // Only hint reference for mutable lvalue reference.
            const bool hintRef = isPassedAsMutableLValueRef(params[i]);

            auto parmName = std::format("{}{}:", params[i]->getName(), hintRef ? "&" : "");
            LabelPart lable{
                .value = tryShrinkHintText(std::move(parmName)),
                .link = AST.toLocalRange(params[i]->getSourceRange()).second,
            };

            auto argBeginLoc = args[i]->getSourceRange().getBegin();
            auto [locFileID, offset] = AST.getDecomposedLoc(argBeginLoc);
            InlayHint hint{
                .kind = kind,
                .offset = offset,
                .labels = {std::move(lable)},
            };

            clang::FileID fileID = interestedOnly ? AST.getInterestedFile() : locFileID;
            result[fileID].push_back(std::move(hint));
        }
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        // Hint local variable, global variable, and structure binding only.
        if(!decl->isLocalVarDecl() && !decl->isFileVarDecl())
            return true;

        if(!option.dedcucedType)
            return true;

        // Hint for indivadual element of structure binding.
        if(auto bind = llvm::dyn_cast<clang::DecompositionDecl>(decl)) {
            for(auto* binding: bind->bindings()) {
                // Hint for used variable only.
                if(auto type = binding->getType(); !type.isNull() && !type->isDependentType()) {
                    // Hint at the end position of identifier.
                    auto name = binding->getName();
                    collectAutoDeclTypeHint(type.getCanonicalType(),
                                            binding->getBeginLoc().getLocWithOffset(name.size()),
                                            decl->getSourceRange(),
                                            Kind::StructureBinding);
                }
            }
            return true;
        }

        /// skip dependent type.
        clang::QualType qty = decl->getType();
        if(qty.isNull() || qty->isDependentType())
            return true;

        if(const auto at = qty->getContainedAutoType()) {
            // Use the first decl as the link of hint string.
            std::optional<clang::SourceRange> originDeclRange;
            if(const auto firstDecl = decl->getFirstDecl())
                originDeclRange = firstDecl->getSourceRange();

            auto tailOfIdentifier = decl->getLocation().getLocWithOffset(decl->getName().size());
            collectAutoDeclTypeHint(qty, tailOfIdentifier, originDeclRange, Kind::AutoDecl);
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
        if(!option.paramName)
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

        llvm::ArrayRef<const clang::Expr*> arguments = {call->getArgs(), call->getNumArgs()};
        if(fndecl)
            // free function
            collectArgumentHint(fndecl->parameters(), arguments, Kind::Parameter);
        else if(auto proto = detectCallViaFnPointer(call->getCallee()); proto.has_value())
            // function pointer
            collectArgumentHint(proto->getParams(), arguments, Kind::Parameter);

        return true;
    }

    bool VisitCXXOperatorCallExpr(const clang::CXXOperatorCallExpr* call) {
        if(!option.paramName)
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

            collectArgumentHint(params, args, Kind::Parameter);
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
        if(!option.paramName)
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

        collectArgumentHint(params, args, Kind::Parameter);
        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* ctor) {
        if(!option.paramName)
            return true;

        // Skip constructor call without an argument list, by checking the validity of
        // getParenOrBraceRange(). Also skip std::initializer_list constructors.
        if(!ctor->getParenOrBraceRange().isValid() || ctor->isStdInitListInitialization())
            return true;

        if(const auto decl = ctor->getConstructor())
            collectArgumentHint(decl->parameters(),
                                {ctor->getArgs(), ctor->getNumArgs()},
                                Kind::Constructor);

        return true;
    }

    void collectReturnTypeHint(clang::SourceLocation hintLoc, clang::QualType retType, Kind kind) {
        std::vector<LabelPart> labels;
        if(!option.typeLink) {
            const auto& policy = AST.context().getPrintingPolicy();

            LabelPart lable;
            lable.value = tryShrinkHintText(std::format("-> {}", retType.getAsString(policy)));
            labels.push_back(std::move(lable));
        } else {
            labels.push_back({.value = "-> "});
            TypeHintLinkBuilder::build(retType, labels, AST);
        }

        auto [locFIleID, offset] = AST.getDecomposedLoc(hintLoc);
        InlayHint hint{
            .kind = kind,
            .offset = offset,
            .labels = std::move(labels),
        };

        clang::FileID fid = interestedOnly ? AST.getInterestedFile() : locFIleID;
        result[fid].push_back(std::move(hint));
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        // 1. Hint block end.
        if(option.blockEnd && decl->isThisDeclarationADefinition() &&
           isMultiLineRange(decl->getSourceRange())) {
            /// FIXME:
            /// Use a proper name such as simplified signature of funtion.
            auto typeLoc = decl->getTypeSourceInfo()->getTypeLoc().getSourceRange();
            auto begin = AST.srcMgr().getCharacterData(typeLoc.getBegin());
            auto end = AST.srcMgr().getCharacterData(typeLoc.getEnd());
            llvm::StringRef source{begin, static_cast<size_t>(end - begin) + 1};

            // Right side of '}'
            collectBlockEndHint(decl->getBodyRBrace().getLocWithOffset(1),
                                std::format("// {}", source),
                                decl->getSourceRange(),
                                Kind::FunctionEnd,
                                DecideDuplicated::Ignore);
        }

        // 2. Hint return type.
        if(!option.returnType)
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
                                      Kind::FunctionReturnType);

        return true;
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* expr) {
        // 1. Hint block end.
        if(option.blockEnd && isMultiLineRange(expr->getBody()->getSourceRange()))
            collectBlockEndHint(
                expr->getEndLoc().getLocWithOffset(1),
                std::format("// lambda #{}", expr->getLambdaClass()->getLambdaManglingNumber()),
                expr->getSourceRange(),
                Kind::LambdaBodyEnd,
                DecideDuplicated::Replace);

        // 2. Hint return type.
        if(!option.returnType)
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
            collectReturnTypeHint(hintLoc, decl->getReturnType(), Kind::LambdaReturnType);

        return true;
    }

    void collectArrayElemIndexHint(int index, clang::SourceLocation location) {
        LabelPart lable{
            .value = std::format("[{}]=", index),  // This shouldn't be shrinked.
            .link = AST.toLocalRange(location).second,
        };

        auto [locFileID, offset] = AST.getDecomposedLoc(location);
        InlayHint hint{
            .kind = Kind::ArrayIndex,
            .offset = offset,
            .labels = {std::move(lable)},
        };

        clang::FileID fileID = interestedOnly ? AST.getInterestedFile() : locFileID;
        result[fileID].push_back(std::move(hint));
    }

    bool VisitInitListExpr(const clang::InitListExpr* Syn) {
        for(int count = 0; auto init: Syn->inits()) {
            if(llvm::isa<clang::DesignatedInitExpr>(init) ||
               hasHandWriteComment(init->getSourceRange()))
                continue;

            // Only hint for the first config.maxArrayElements elements.
            if(count++ >= option.maxArrayElements)
                break;

            collectArrayElemIndexHint(count, init->getBeginLoc());
        }
        return true;
    }

    bool isMultiLineRange(const clang::SourceRange range) {
        const auto& SM = AST.srcMgr();
        return range.isValid() && SM.getPresumedLineNumber(range.getBegin()) <
                                      SM.getPresumedLineNumber(range.getEnd());
    }

    llvm::StringRef remainTextOfThatLine(clang::SourceLocation location) {
        auto [_, offset] = AST.getDecomposedLoc(location);
        auto remain = code.substr(offset).split('\n').first;
        return remain.ltrim();
    }

    /// This enum decide how to handle the duplicated block-end hint in the same line.
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
        //      };} |// struct Out|;
        Ignore,
    };

    void collectBlockEndHint(clang::SourceLocation endLoc,
                             std::string text,
                             clang::SourceRange linkRange,
                             Kind kind,
                             DecideDuplicated decision) {
        // Already has a comment in that line.
        if(auto remain = remainTextOfThatLine(endLoc);
           remain.starts_with("/*") || remain.starts_with("//"))
            return;

        const auto& SM = AST.srcMgr();
        auto fileID = interestedOnly ? AST.getInterestedFile() : SM.getDecomposedLoc(endLoc).first;
        auto& state = result[fileID];

        if(decision != DecideDuplicated::AcceptBoth && !state.empty()) {
            // Already has a duplicated hint in that line, use the newer hint instead.
            auto lastHintLine = SM.getLineNumber(fileID, state.back().offset);
            auto thatLine = SM.getPresumedLineNumber(endLoc);
            if(lastHintLine == thatLine) {
                if(decision == DecideDuplicated::Replace)
                    state.pop_back();
                else
                    return;  // use the old one.
            }
        }

        LabelPart lable{
            .value = tryShrinkHintText(std::move(text)),
            .link = AST.toLocalRange(linkRange).second,
        };

        InlayHint hint{
            .kind = kind,
            .offset = AST.getDecomposedLoc(endLoc).second,
            .labels = {std::move(lable)},
        };

        state.push_back(std::move(hint));
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        if(!option.blockEnd)
            return true;

        auto range = decl->getSourceRange();
        if(decl->isAnonymousNamespace() || !isMultiLineRange(range))
            return true;

        collectBlockEndHint(decl->getRBraceLoc().getLocWithOffset(1),
                            std::format("// namespace {}", decl->getName()),
                            range,
                            Kind::NamespaceEnd,
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

        LabelPart lable{
            .value = tryShrinkHintText(std::format("size: {}, align: {}", size, align)),
            .link = AST.toLocalRange(decl->getSourceRange()).second,
        };

        // right side of identifier.
        auto tail = decl->getLocation().getLocWithOffset(decl->getName().size());
        auto [locFildID, offset] = AST.getDecomposedLoc(tail);
        InlayHint hint{
            .kind = Kind::StructSizeAndAlign,
            .offset = offset,
            .labels = {std::move(lable)},
        };

        auto fid = interestedOnly ? AST.getInterestedFile() : locFildID;
        result[fid].push_back(std::move(hint));
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        if(!decl->isThisDeclarationADefinition())
            return true;

        if(option.blockEnd && isMultiLineRange(decl->getBraceRange())) {
            std::string hintText = std::format("// {}", decl->getKindName().str());
            // Add a tail flag for enum declaration as clangd's do.
            if(const auto* enumDecl = llvm::dyn_cast<clang::EnumDecl>(decl);
               enumDecl && enumDecl->isScoped())
                hintText += enumDecl->isScopedUsingClassTag() ? " class" : " struct";

            // Format text to 'struct Example' or `class Example` or `enum class Example`
            hintText.append(" ").append(getDeclName(decl));
            collectBlockEndHint(decl->getBraceRange().getEnd().getLocWithOffset(1),
                                std::move(hintText),
                                decl->getSourceRange(),
                                Kind::TagDeclEnd,
                                DecideDuplicated::Ignore);
        }

        if(option.structSizeAndAlign)
            collectStructSizeAndAlign(decl);

        return true;
    }

    /// TODO:
    /// Find proper end location of cast expression.
    // bool VisitImplicitCastExpr(const clang::ImplicitCastExpr* stmt) {
    //     if(!config.implicitCast)
    //         return true;
    //     if(auto* expr = llvm::dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    //         Lable lable{
    //             .value = shrinkText(std::format("as {}", expr->getType().getAsString(policy))),
    //             .tooltip = blank(),
    //         };
    //         // right side of that expr.
    //         InlayHint hint{
    //             .position = cvtr.toPosition(stmt->getEndLoc()),
    //             .lable = {std::move(lable)},
    //             .kind = proto::InlayHintKind::Parameter,
    //         };
    //         result.push_back(std::move(hint));
    //     }
    //     return true;
    // }

    static Storage collect(ASTInfo& AST,
                           bool interestedOnly,
                           std::optional<LocalSourceRange> limit,
                           const config::InlayHintOption& option) {
        InlayHintCollector collector{AST, interestedOnly, limit, option};
        collector.TraverseTranslationUnitDecl(AST.tu());
        return std::move(collector.result);
    }
};

// bool isAvailableWithOption(InlayHintKind kind, const config::InlayHintOption& config) {
//     using enum InlayHintKind::Kind;

//     switch(kind.kind()) {
//         case Invalid: return false;

//         case AutoDecl:
//         case StructureBinding: return config.dedcucedType;

//         case Parameter:
//         case Constructor: return config.paramName;

//         case FunctionReturnType:
//         case LambdaReturnType: return config.returnType;

//         case IfBlockEnd:
//         case SwitchBlockEnd:
//         case WhileBlockEnd:
//         case ForBlockEnd:
//         case NamespaceEnd:
//         case TagDeclEnd:
//         case FunctionEnd:
//         case LambdaBodyEnd: return config.blockEnd;

//         case ArrayIndex: return config.maxArrayElements > 0;
//         case StructSizeAndAlign: return config.structSizeAndAlign;
//         case MemberSizeAndOffset: return config.memberSizeAndOffset;
//         case ImplicitCast: return config.implicitCast;
//         case ChainCall: return config.chainCall;
//         case NumberLiteralToHex: return config.numberLiteralToHex;
//         case CStrLength: return config.cstrLength;
//     }
// }

}  // namespace

std::vector<InlayHint> inlayHints(ASTInfo& AST,
                                  proto::Range range,
                                  const config::InlayHintOption& option) {
    assert(range.start != range.end && "Invalid range from client.");

    // Take 0-0 based Lsp Location from `param.range` and convert it to offset pair.
    auto fromLineCol = [&](proto::Position pos) -> clang::SourceLocation {
        return AST.srcMgr().translateLineCol(AST.getInterestedFile(),
                                             pos.line + 1,
                                             pos.character + 1);
    };
    clang::SourceRange requestRange{fromLineCol(range.start), fromLineCol(range.end)};
    assert(requestRange.isValid() && "Invalid SourceRange.");

    /// TODO:
    /// Check and fix invalid options before collecting hints.

    auto limit = AST.toLocalRange(requestRange).second;
    auto result = InlayHintCollector::collect(AST, /*interestedOnly=*/true, limit, option);
    return std::move(result[AST.getInterestedFile()]);
}

index::Shared<std::vector<InlayHint>> indexInlayHints(ASTInfo& AST) {
    constexpr config::InlayHintOption EnableAll{
        .maxLength = 0,
        .maxArrayElements = 0,
        .dedcucedType = true,
        .returnType = true,
        .blockEnd = true,
        .paramName = true,
        .typeLink = true,
        .structSizeAndAlign = true,
    };
    return InlayHintCollector::collect(AST, /*interestedOnly=*/false, std::nullopt, EnableAll);
}

}  // namespace clice::feature

