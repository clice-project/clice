#include "AST/FilterASTVisitor.h"
#include "Feature/InlayHint.h"
#include "Support/Compare.h"
#include "AST/Utility.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringExtras.h"

namespace clice::feature {

namespace {

using namespace clang;

// For now, inlay hints are always anchored at the left or right of their range.
enum class HintSide { Left, Right };

void stripLeadingUnderscores(StringRef& Name) {
    Name = Name.ltrim('_');
}

bool isExpandedFromParameterPack(const ParmVarDecl* D) {
    return ast::getUnderlyingPackType(D) != nullptr;
}

ArrayRef<const ParmVarDecl*>
    maybeDropCxxExplicitObjectParameters(ArrayRef<const ParmVarDecl*> Params) {
    if(!Params.empty() && Params.front()->isExplicitObjectParameter())
        Params = Params.drop_front(1);
    return Params;
}

template <typename R>
std::string joinAndTruncate(const R& Range, size_t MaxLength) {
    std::string Out;
    llvm::raw_string_ostream OS(Out);
    llvm::ListSeparator Sep(", ");
    for(auto&& Element: Range) {
        OS << Sep;
        if(Out.size() + Element.size() >= MaxLength) {
            OS << "...";
            break;
        }
        OS << Element;
    }
    OS.flush();
    return Out;
}

// for a ParmVarDecl from a function declaration, returns the corresponding
// ParmVarDecl from the definition if possible, nullptr otherwise.
const static ParmVarDecl* getParamDefinition(const ParmVarDecl* P) {
    if(auto* Callee = dyn_cast<FunctionDecl>(P->getDeclContext())) {
        if(auto* Def = Callee->getDefinition()) {
            auto I = std::distance(Callee->param_begin(), llvm::find(Callee->parameters(), P));
            if(I < (int)Callee->getNumParams()) {
                return Def->getParamDecl(I);
            }
        }
    }
    return nullptr;
}

// If "E" spells a single unqualified identifier, return that name.
// Otherwise, return an empty string.
static StringRef getSpelledIdentifier(const Expr* E) {
    E = E->IgnoreUnlessSpelledInSource();

    if(auto* DRE = dyn_cast<DeclRefExpr>(E))
        if(!DRE->getQualifier())
            return ast::getSimpleName(*DRE->getDecl());

    if(auto* ME = dyn_cast<MemberExpr>(E))
        if(!ME->getQualifier() && ME->isImplicitAccess())
            return ast::getSimpleName(*ME->getMemberDecl());

    return {};
}

using NameVec = SmallVector<StringRef, 8>;

static bool isSetter(const FunctionDecl* Callee, const NameVec& ParamNames) {
    if(ParamNames.size() != 1)
        return false;

    StringRef Name = ast::getSimpleName(*Callee);
    if(!Name.starts_with_insensitive("set"))
        return false;

    // In addition to checking that the function has one parameter and its
    // name starts with "set", also check that the part after "set" matches
    // the name of the parameter (ignoring case). The idea here is that if
    // the parameter name differs, it may contain extra information that
    // may be useful to show in a hint, as in:
    //   void setTimeout(int timeoutMillis);
    // This currently doesn't handle cases where params use snake_case
    // and functions don't, e.g.
    //   void setExceptionHandler(EHFunc exception_handler);
    // We could improve this by replacing `equals_insensitive` with some
    // `sloppy_equals` which ignores case and also skips underscores.
    StringRef WhatItIsSetting = Name.substr(3).ltrim("_");
    return WhatItIsSetting.equals_insensitive(ParamNames[0]);
}

// Checks if the callee is one of the builtins
// addressof, as_const, forward, move(_if_noexcept)
static bool isSimpleBuiltin(const FunctionDecl* Callee) {
    switch(Callee->getBuiltinID()) {
        case Builtin::BIaddressof:
        case Builtin::BIas_const:
        case Builtin::BIforward:
        case Builtin::BImove:
        case Builtin::BImove_if_noexcept: return true;
        default: return false;
    }
}

struct Callee {
    // Only one of Decl or Loc is set.
    // Loc is for calls through function pointers.
    const FunctionDecl* Decl = nullptr;
    FunctionProtoTypeLoc Loc;
};

class InlayHintVisitor : public FilteredASTVisitor<InlayHintVisitor> {
public:
    using Base = FilteredASTVisitor<InlayHintVisitor>;

    InlayHintVisitor(std::vector<InlayHint>& Results,
                     CompilationUnit& unit,
                     std::optional<LocalSourceRange> restrict_range) :
        Base(unit, true), results(Results), unit(unit), AST(unit.context()),

        Tokens(unit.token_buffer()), restrict_range(*std::move(restrict_range)),
        MainFileID(AST.getSourceManager().getMainFileID()),
        TypeHintPolicy(this->AST.getPrintingPolicy()) {

        MainFileBuf = unit.interested_content();

        TypeHintPolicy.SuppressScope = true;           // keep type names short
        TypeHintPolicy.AnonymousTagLocations = false;  // do not print lambda locations

        // Not setting PrintCanonicalTypes for "auto" allows
        // SuppressDefaultTemplateArgs (set by default) to have an effect.
    }

public:
    // Get the range of the main file that *exactly* corresponds to R.
    std::optional<LocalSourceRange> getHintRange(SourceRange R) {
        const auto& SM = AST.getSourceManager();
        auto Spelled = Tokens.spelledForExpanded(Tokens.expandedTokens(R));
        // TokenBuffer will return null if e.g. R corresponds to only part of a
        // macro expansion.
        if(!Spelled || Spelled->empty())
            return std::nullopt;

        // Hint must be within the main file, not e.g. a non-preamble include.
        if(SM.getFileID(Spelled->front().location()) != SM.getMainFileID() ||
           SM.getFileID(Spelled->back().location()) != SM.getMainFileID())
            return std::nullopt;

        /// FIXME:
        // return LocalSourceRange{sourceLocToPosition(SM, Spelled->front().location()),
        //              sourceLocToPosition(SM, Spelled->back().endLocation())};

        return unit
            .decompose_range(
                clang::SourceRange(Spelled->front().location(), Spelled->back().endLocation()))
            .second;
    }

    void addBlockEndHint(SourceRange BraceRange,
                         StringRef DeclPrefix,
                         StringRef Name,
                         StringRef OptionalPunctuation) {
        auto HintRange = computeBlockEndHintRange(BraceRange, OptionalPunctuation);
        if(!HintRange)
            return;

        std::string Label = DeclPrefix.str();
        if(!Label.empty() && !Name.empty())
            Label += ' ';
        Label += Name;

        constexpr unsigned HintMaxLengthLimit = 60;
        if(Label.length() > HintMaxLengthLimit)
            return;

        add_inlay_hint(*HintRange, HintSide::Right, InlayHintKind::BlockEnd, " // ", Label, "");
    }

    // Compute the LSP range to attach the block end hint to, if any allowed.
    // 1. "}" is the last non-whitespace character on the line. The range of "}"
    // is returned.
    // 2. After "}", if the trimmed trailing text is exactly
    // `OptionalPunctuation`, say ";". The range of "} ... ;" is returned.
    // Otherwise, the hint shouldn't be shown.
    std::optional<LocalSourceRange> computeBlockEndHintRange(SourceRange BraceRange,
                                                             StringRef OptionalPunctuation) {
        constexpr unsigned HintMinLineLimit = 2;

        auto& SM = AST.getSourceManager();
        auto [BlockBeginFileId, BlockBeginOffset] =
            SM.getDecomposedLoc(SM.getFileLoc(BraceRange.getBegin()));
        auto RBraceLoc = SM.getFileLoc(BraceRange.getEnd());
        auto [RBraceFileId, RBraceOffset] = SM.getDecomposedLoc(RBraceLoc);

        // Because we need to check the block satisfies the minimum line limit, we
        // require both source location to be in the main file. This prevents hint
        // to be shown in weird cases like '{' is actually in a "#include", but it's
        // rare anyway.
        if(BlockBeginFileId != MainFileID || RBraceFileId != MainFileID)
            return std::nullopt;

        StringRef RestOfLine = MainFileBuf.substr(RBraceOffset).split('\n').first;
        if(!RestOfLine.starts_with("}"))
            return std::nullopt;

        StringRef TrimmedTrailingText = RestOfLine.drop_front().trim();
        if(!TrimmedTrailingText.empty() && TrimmedTrailingText != OptionalPunctuation)
            return std::nullopt;

        auto BlockBeginLine = SM.getLineNumber(BlockBeginFileId, BlockBeginOffset);
        auto RBraceLine = SM.getLineNumber(RBraceFileId, RBraceOffset);

        // Don't show hint on trivial blocks like `class X {};`
        if(BlockBeginLine + HintMinLineLimit - 1 > RBraceLine)
            return std::nullopt;

        // This is what we attach the hint to, usually "}" or "};".
        StringRef HintRangeText =
            RestOfLine.take_front(TrimmedTrailingText.empty()
                                      ? 1
                                      : TrimmedTrailingText.bytes_end() - RestOfLine.bytes_begin());

        /// FIXME: Handle case, if RBraceLoc is from macro expansion.
        auto [fid, offset] = unit.decompose_location(RBraceLoc);
        return LocalSourceRange(offset, offset + HintRangeText.size());
    }

    // We pass HintSide rather than SourceLocation because we want to ensure
    // it is in the same file as the common file range.
    void add_inlay_hint(SourceRange R,
                        HintSide Side,
                        InlayHintKind Kind,
                        llvm::StringRef Prefix,
                        llvm::StringRef Label,
                        llvm::StringRef Suffix) {
        auto LSPRange = getHintRange(R);
        if(!LSPRange)
            return;

        add_inlay_hint(*LSPRange, Side, Kind, Prefix, Label, Suffix);
    }

    void add_inlay_hint(LocalSourceRange LSPRange,
                        HintSide side,
                        InlayHintKind kind,
                        llvm::StringRef prefix,
                        llvm::StringRef Label,
                        llvm::StringRef suffix) {
        // We shouldn't get as far as adding a hint if the category is disabled.
        // We'd like to disable as much of the analysis as possible above instead.
        // Assert in debug mode but add a dynamic check in production.
        assert(options.enabled && "Shouldn't get here if disabled!");

        std::uint32_t offset = side == HintSide::Left ? LSPRange.begin : LSPRange.end;
        if(restrict_range.valid() && !restrict_range.contains(offset))
            return;

        bool pad_left = prefix.consume_front(" ");
        bool pad_right = suffix.consume_back(" ");

        InlayHint hint{offset, kind};
        hint.parts.emplace_back(-1, (prefix + Label + suffix).str());
        results.push_back(std::move(hint));
    }

    void add_type_hint(SourceRange R, QualType T, llvm::StringRef Prefix) {
        if(!options.deduced_types || T.isNull())
            return;

        // The sugared type is more useful in some cases, and the canonical
        // type in other cases.
        auto Desugared = ast::maybeDesugar(AST, T);
        std::string TypeName = Desugared.getAsString(TypeHintPolicy);

        auto should_print = [&](llvm::StringRef TypeName) {
            return options.type_name_limit == 0 || TypeName.size() < options.type_name_limit;
        };

        if(T != Desugared && !should_print(TypeName)) {
            // If the desugared type is too long to display, fallback to the sugared
            // type.
            TypeName = T.getAsString(TypeHintPolicy);
        }

        if(should_print(TypeName))
            add_inlay_hint(R,
                           HintSide::Right,
                           InlayHintKind::Type,
                           Prefix,
                           TypeName,
                           /*Suffix=*/"");
    }

    void addDesignatorHint(SourceRange R, llvm::StringRef Text) {
        add_inlay_hint(R,
                       HintSide::Left,
                       InlayHintKind::Designator,
                       /*Prefix=*/"",
                       Text,
                       /*Suffix=*/"=");
    }

    void add_return_type_hint(FunctionDecl* D, SourceRange Range) {
        auto* AT = D->getReturnType()->getContainedAutoType();
        if(!AT || AT->getDeducedType().isNull())
            return;
        add_type_hint(Range, D->getReturnType(), /*Prefix=*/"-> ");
    }

    bool shouldHintName(const Expr* Arg, StringRef ParamName) {
        if(ParamName.empty())
            return false;

        // If the argument expression is a single name and it matches the
        // parameter name exactly, omit the name hint.
        if(ParamName == getSpelledIdentifier(Arg))
            return false;

        // Exclude argument expressions preceded by a /*paramName*/.
        if(isPrecededByParamNameComment(Arg, ParamName))
            return false;

        return true;
    }

    bool shouldHintReference(const ParmVarDecl* Param, const ParmVarDecl* ForwardedParam) {
        // We add a & hint only when the argument is passed as mutable reference.
        // For parameters that are not part of an expanded pack, this is
        // straightforward. For expanded pack parameters, it's likely that they will
        // be forwarded to another function. In this situation, we only want to add
        // the reference hint if the argument is actually being used via mutable
        // reference. This means we need to check
        // 1. whether the value category of the argument is preserved, i.e. each
        //    pack expansion uses std::forward correctly.
        // 2. whether the argument is ever copied/cast instead of passed
        //    by-reference
        // Instead of checking this explicitly, we use the following proxy:
        // 1. the value category can only change from rvalue to lvalue during
        //    forwarding, so checking whether both the parameter of the forwarding
        //    function and the forwarded function are lvalue references detects such
        //    a conversion.
        // 2. if the argument is copied/cast somewhere in the chain of forwarding
        //    calls, it can only be passed on to an rvalue reference or const lvalue
        //    reference parameter. Thus if the forwarded parameter is a mutable
        //    lvalue reference, it cannot have been copied/cast to on the way.
        // Additionally, we should not add a reference hint if the forwarded
        // parameter was only partially resolved, i.e. points to an expanded pack
        // parameter, since we do not know how it will be used eventually.
        auto Type = Param->getType();
        auto ForwardedType = ForwardedParam->getType();
        return Type->isLValueReferenceType() && ForwardedType->isLValueReferenceType() &&
               !ForwardedType.getNonReferenceType().isConstQualified() &&
               !isExpandedFromParameterPack(ForwardedParam);
    }

    void markBlockEnd(const Stmt* Body, llvm::StringRef Label, llvm::StringRef Name = "") {
        if(const auto* CS = llvm::dyn_cast_or_null<CompoundStmt>(Body))
            addBlockEndHint(CS->getSourceRange(), Label, Name, "");
    }

    using NameVec = SmallVector<StringRef, 8>;

    void processCall(Callee Callee,
                     SourceLocation RParenOrBraceLoc,
                     llvm::ArrayRef<const Expr*> Args) {
        assert(Callee.Decl || Callee.Loc);

        if((!options.parameters && !options.default_arguments) || Args.size() == 0)
            return;

        // The parameter name of a move or copy constructor is not very interesting.
        if(Callee.Decl)
            if(auto* Ctor = dyn_cast<CXXConstructorDecl>(Callee.Decl))
                if(Ctor->isCopyOrMoveConstructor())
                    return;

        SmallVector<std::string> FormattedDefaultArgs;
        bool HasNonDefaultArgs = false;

        ArrayRef<const ParmVarDecl*> Params, ForwardedParams;
        // Resolve parameter packs to their forwarded parameter
        SmallVector<const ParmVarDecl*> ForwardedParamsStorage;
        if(Callee.Decl) {
            Params = maybeDropCxxExplicitObjectParameters(Callee.Decl->parameters());
            ForwardedParamsStorage = ast::resolveForwardingParameters(Callee.Decl);
            ForwardedParams = maybeDropCxxExplicitObjectParameters(ForwardedParamsStorage);
        } else {
            Params = maybeDropCxxExplicitObjectParameters(Callee.Loc.getParams());
            ForwardedParams = {Params.begin(), Params.end()};
        }

        NameVec ParameterNames = chooseParameterNames(ForwardedParams);

        // Exclude setters (i.e. functions with one argument whose name begins with
        // "set"), and builtins like std::move/forward/... as their parameter name
        // is also not likely to be interesting.
        if(Callee.Decl && (isSetter(Callee.Decl, ParameterNames) || isSimpleBuiltin(Callee.Decl)))
            return;

        for(size_t I = 0; I < ParameterNames.size() && I < Args.size(); ++I) {
            // Pack expansion expressions cause the 1:1 mapping between arguments and
            // parameters to break down, so we don't add further inlay hints if we
            // encounter one.
            if(isa<PackExpansionExpr>(Args[I])) {
                break;
            }

            StringRef Name = ParameterNames[I];
            const bool NameHint = shouldHintName(Args[I], Name) && options.parameters;
            const bool ReferenceHint =
                shouldHintReference(Params[I], ForwardedParams[I]) && options.parameters;

            const bool IsDefault = isa<CXXDefaultArgExpr>(Args[I]);
            HasNonDefaultArgs |= !IsDefault;
            if(IsDefault) {
                if(options.default_arguments) {
                    const auto SourceText = clang::Lexer::getSourceText(
                        CharSourceRange::getTokenRange(Params[I]->getDefaultArgRange()),
                        AST.getSourceManager(),
                        AST.getLangOpts());
                    const auto Abbrev =
                        (SourceText.size() > options.type_name_limit || SourceText.contains("\n"))
                            ? "..."
                            : SourceText;
                    if(NameHint)
                        FormattedDefaultArgs.emplace_back(llvm::formatv("{0}: {1}", Name, Abbrev));
                    else
                        FormattedDefaultArgs.emplace_back(llvm::formatv("{0}", Abbrev));
                }
            } else if(NameHint || ReferenceHint) {
                add_inlay_hint(Args[I]->getSourceRange(),
                               HintSide::Left,
                               InlayHintKind::Parameter,
                               ReferenceHint ? "&" : "",
                               NameHint ? Name : "",
                               ": ");
            }
        }

        if(!FormattedDefaultArgs.empty()) {
            std::string Hint = joinAndTruncate(FormattedDefaultArgs, options.type_name_limit);
            add_inlay_hint(SourceRange{RParenOrBraceLoc},
                           HintSide::Left,
                           InlayHintKind::DefaultArgument,
                           HasNonDefaultArgs ? ", " : "",
                           Hint,
                           "");
        }
    }

    // Checks if "E" is spelled in the main file and preceded by a C-style comment
    // whose contents match ParamName (allowing for whitespace and an optional "="
    // at the end.
    bool isPrecededByParamNameComment(const Expr* E, StringRef ParamName) {
        auto& SM = AST.getSourceManager();
        auto FileLoc = SM.getFileLoc(E->getBeginLoc());
        auto Decomposed = SM.getDecomposedLoc(FileLoc);
        if(Decomposed.first != MainFileID)
            return false;

        StringRef SourcePrefix = MainFileBuf.substr(0, Decomposed.second);
        // Allow whitespace between comment and expression.
        SourcePrefix = SourcePrefix.rtrim();
        // Check for comment ending.
        if(!SourcePrefix.consume_back("*/"))
            return false;
        // Ignore some punctuation and whitespace around comment.
        // In particular this allows designators to match nicely.
        llvm::StringLiteral IgnoreChars = " =.";
        SourcePrefix = SourcePrefix.rtrim(IgnoreChars);
        ParamName = ParamName.trim(IgnoreChars);
        // Other than that, the comment must contain exactly ParamName.
        if(!SourcePrefix.consume_back(ParamName))
            return false;
        SourcePrefix = SourcePrefix.rtrim(IgnoreChars);
        return SourcePrefix.ends_with("/*");
    }

    NameVec chooseParameterNames(ArrayRef<const ParmVarDecl*> Parameters) {
        NameVec ParameterNames;
        for(const auto* P: Parameters) {
            if(isExpandedFromParameterPack(P)) {
                // If we haven't resolved a pack paramater (e.g. foo(Args... args)) to a
                // non-pack parameter, then hinting as foo(args: 1, args: 2, args: 3) is
                // unlikely to be useful.
                ParameterNames.emplace_back();
            } else {
                auto SimpleName = ast::getSimpleName(*P);
                // If the parameter is unnamed in the declaration:
                // attempt to get its name from the definition
                if(SimpleName.empty()) {
                    if(const auto* PD = getParamDefinition(P)) {
                        SimpleName = ast::getSimpleName(*PD);
                    }
                }
                ParameterNames.emplace_back(SimpleName);
            }
        }

        // Standard library functions often have parameter names that start
        // with underscores, which makes the hints noisy, so strip them out.
        for(auto& Name: ParameterNames)
            stripLeadingUnderscores(Name);

        return ParameterNames;
    }

    ParmVarDecl* getOnlyParamInstantiation(ParmVarDecl* D) {
        auto* TemplateFunction = llvm::dyn_cast<FunctionDecl>(D->getDeclContext());
        if(!TemplateFunction)
            return nullptr;
        auto* InstantiatedFunction =
            llvm::dyn_cast_or_null<FunctionDecl>(ast::get_only_instantiation(TemplateFunction));
        if(!InstantiatedFunction)
            return nullptr;

        unsigned ParamIdx = 0;
        for(auto* Param: TemplateFunction->parameters()) {
            // Can't reason about param indexes in the presence of preceding packs.
            // And if this param is a pack, it may expand to multiple params.
            if(Param->isParameterPack())
                return nullptr;
            if(Param == D)
                break;
            ++ParamIdx;
        }
        assert(ParamIdx < TemplateFunction->getNumParams() && "Couldn't find param in list?");
        assert(ParamIdx < InstantiatedFunction->getNumParams() &&
               "Instantiated function has fewer (non-pack) parameters?");
        return InstantiatedFunction->getParamDecl(ParamIdx);
    }

public:
    // Carefully recurse into PseudoObjectExprs, which typically incorporate
    // a syntactic expression and several semantic expressions.
    bool TraversePseudoObjectExpr(clang::PseudoObjectExpr* expr) {
        clang::Expr* syntactic_expr = expr->getSyntacticForm();
        if(llvm::isa<clang::CallExpr>(syntactic_expr)) {
            // Since the counterpart semantics usually get the identical source
            // locations as the syntactic one, visiting those would end up presenting
            // confusing hints e.g., __builtin_dump_struct.
            // Thus, only traverse the syntactic forms if this is written as a
            // CallExpr. This leaves the door open in case the arguments in the
            // syntactic form could possibly get parameter names.
            return Base::TraverseStmt(syntactic_expr);
        }

        // We don't want the hints for some of the MS property extensions.
        // e.g.
        // struct S {
        //   __declspec(property(get=GetX, put=PutX)) int x[];
        //   void PutX(int y);
        //   void Work(int y) { x = y; } // Bad: `x = y: y`.
        // };
        if(llvm::isa<clang::BinaryOperator>(syntactic_expr)) {
            return true;
        }

        // FIXME: Handle other forms of a pseudo object expression.
        return Base::TraversePseudoObjectExpr(expr);
    }

    bool VisitNamespaceDecl(NamespaceDecl* D) {
        if(options.block_end) {
            // For namespace, the range actually starts at the namespace keyword. But
            // it should be fine since it's usually very short.
            addBlockEndHint(D->getSourceRange(), "namespace", ast::getSimpleName(*D), "");
        }
        return true;
    }

    bool VisitTagDecl(TagDecl* D) {
        if(options.block_end && D->isThisDeclarationADefinition()) {
            std::string DeclPrefix = D->getKindName().str();
            if(const auto* ED = dyn_cast<EnumDecl>(D)) {
                if(ED->isScoped())
                    DeclPrefix += ED->isScopedUsingClassTag() ? " class" : " struct";
            };
            addBlockEndHint(D->getBraceRange(), DeclPrefix, ast::getSimpleName(*D), ";");
        }
        return true;
    }

    bool VisitFunctionDecl(FunctionDecl* D) {
        if(auto* FPT = llvm::dyn_cast<FunctionProtoType>(D->getType().getTypePtr())) {
            if(!FPT->hasTrailingReturn()) {
                if(auto FTL = D->getFunctionTypeLoc())
                    add_return_type_hint(D, FTL.getRParenLoc());
            }
        }

        if(options.block_end && D->isThisDeclarationADefinition()) {
            // We use `printName` here to properly print name of ctor/dtor/operator
            // overload.
            if(const Stmt* Body = D->getBody())
                addBlockEndHint(Body->getSourceRange(), "", ast::print_name(D), "");
        }

        return true;
    }

    bool VisitVarDecl(VarDecl* D) {
        // Do not show hints for the aggregate in a structured binding,
        // but show hints for the individual bindings.
        if(auto* DD = dyn_cast<DecompositionDecl>(D)) {
            for(auto* Binding: DD->bindings()) {
                // For structured bindings, print canonical types. This is important
                // because for bindings that use the tuple_element protocol, the
                // non-canonical types would be "tuple_element<I, A>::type".
                if(auto Type = Binding->getType(); !Type.isNull() && !Type->isDependentType())
                    add_type_hint(Binding->getLocation(),
                                  Type.getCanonicalType(),
                                  /*Prefix=*/": ");
            }
            return true;
        }

        if(auto* AT = D->getType()->getContainedAutoType()) {
            if(AT->isDeduced() && !D->getType()->isDependentType()) {
                // Our current approach is to place the hint on the variable
                // and accordingly print the full type
                // (e.g. for `const auto& x = 42`, print `const int&`).
                // Alternatively, we could place the hint on the `auto`
                // (and then just print the type deduced for the `auto`).
                add_type_hint(D->getLocation(), D->getType(), /*Prefix=*/": ");
            }
        }

        // Handle templates like `int foo(auto x)` with exactly one instantiation.
        if(auto* PVD = llvm::dyn_cast<ParmVarDecl>(D)) {
            if(D->getIdentifier() && PVD->getType()->isDependentType() &&
               !ast::get_contained_auto_param_type(D->getTypeSourceInfo()->getTypeLoc()).isNull()) {
                if(auto* IPVD = getOnlyParamInstantiation(PVD))
                    add_type_hint(D->getLocation(), IPVD->getType(), /*Prefix=*/": ");
            }
        }

        return true;
    }

    bool VisitCXXConstructExpr(CXXConstructExpr* E) {
        // Weed out constructor calls that don't look like a function call with
        // an argument list, by checking the validity of getParenOrBraceRange().
        // Also weed out std::initializer_list constructors as there are no names
        // for the individual arguments.
        if(!E->getParenOrBraceRange().isValid() || E->isStdInitListInitialization()) {
            return true;
        }

        Callee Callee;
        Callee.Decl = E->getConstructor();
        if(!Callee.Decl)
            return true;
        processCall(Callee, E->getParenOrBraceRange().getEnd(), {E->getArgs(), E->getNumArgs()});
        return true;
    }

    bool VisitCallExpr(CallExpr* E) {
        if(!options.parameters)
            return true;

        auto isFunctionObjectCallExpr = [](CallExpr* E) {
            if(auto* CallExpr = dyn_cast<CXXOperatorCallExpr>(E)) {
                return CallExpr->getOperator() == OverloadedOperatorKind::OO_Call;
            }

            return false;
        };

        bool IsFunctor = isFunctionObjectCallExpr(E);

        // Do not show parameter hints for user-defined literals or
        // operator calls except for operator(). (Among other reasons, the resulting
        // hints can look awkward, e.g. the expression can itself be a function
        // argument and then we'd get two hints side by side).
        if((isa<CXXOperatorCallExpr>(E) && !IsFunctor) || isa<UserDefinedLiteral>(E))
            return true;

        /// FIXME: Use template resolver here.
        if(E->isTypeDependent() || E->isValueDependent()) {
            return true;
        }

        auto CalleeDecl = E->getCalleeDecl();

        Callee Callee;
        if(const auto* FD = dyn_cast<FunctionDecl>(CalleeDecl))
            Callee.Decl = FD;
        else if(const auto* FTD = dyn_cast<FunctionTemplateDecl>(CalleeDecl))
            Callee.Decl = FTD->getTemplatedDecl();
        else if(FunctionProtoTypeLoc Loc = ast::getPrototypeLoc(E->getCallee()))
            Callee.Loc = Loc;
        else
            return true;

        // N4868 [over.call.object]p3 says,
        // The argument list submitted to overload resolution consists of the
        // argument expressions present in the function call syntax preceded by the
        // implied object argument (E).
        //
        // As well as the provision from P0847R7 Deducing This [expr.call]p7:
        // ...If the function is an explicit object member function and there is an
        // implied object argument ([over.call.func]), the list of provided
        // arguments is preceded by the implied object argument for the purposes of
        // this correspondence...
        llvm::ArrayRef<const Expr*> Args = {E->getArgs(), E->getNumArgs()};
        // We don't have the implied object argument through a function pointer
        // either.
        if(const CXXMethodDecl* Method = dyn_cast_or_null<CXXMethodDecl>(Callee.Decl))
            if(IsFunctor || Method->hasCXXExplicitFunctionObjectParameter())
                Args = Args.drop_front(1);
        processCall(Callee, E->getRParenLoc(), Args);
        return true;
    }

    bool VisitForStmt(ForStmt* S) {
        if(options.block_end) {
            std::string Name;
            // Common case: for (int I = 0; I < N; I++). Use "I" as the name.
            if(auto* DS = llvm::dyn_cast_or_null<DeclStmt>(S->getInit()); DS && DS->isSingleDecl())
                Name = ast::getSimpleName(llvm::cast<NamedDecl>(*DS->getSingleDecl()));
            else
                Name = ast::summarizeExpr(S->getCond());
            markBlockEnd(S->getBody(), "for", Name);
        }
        return true;
    }

    bool VisitCXXForRangeStmt(CXXForRangeStmt* S) {
        if(options.block_end)
            markBlockEnd(S->getBody(), "for", ast::getSimpleName(*S->getLoopVariable()));
        return true;
    }

    bool VisitWhileStmt(WhileStmt* S) {
        if(options.block_end)
            markBlockEnd(S->getBody(), "while", ast::summarizeExpr(S->getCond()));
        return true;
    }

    bool VisitSwitchStmt(SwitchStmt* S) {
        if(options.block_end)
            markBlockEnd(S->getBody(), "switch", ast::summarizeExpr(S->getCond()));
        return true;
    }

    bool VisitIfStmt(IfStmt* S) {
        if(options.block_end) {
            if(const auto* ElseIf = llvm::dyn_cast_or_null<IfStmt>(S->getElse()))
                ElseIfs.insert(ElseIf);
            // Don't use markBlockEnd: the relevant range is [then.begin, else.end].
            if(const auto* EndCS =
                   llvm::dyn_cast<CompoundStmt>(S->getElse() ? S->getElse() : S->getThen())) {
                addBlockEndHint({S->getThen()->getBeginLoc(), EndCS->getRBracLoc()},
                                "if",
                                ElseIfs.contains(S) ? "" : ast::summarizeExpr(S->getCond()),
                                "");
            }
        }
        return true;
    }

    bool VisitLambdaExpr(LambdaExpr* E) {
        FunctionDecl* D = E->getCallOperator();
        if(!E->hasExplicitResultType()) {
            SourceLocation TypeHintLoc;
            if(!E->hasExplicitParameters())
                TypeHintLoc = E->getIntroducerRange().getEnd();
            else if(auto FTL = D->getFunctionTypeLoc())
                TypeHintLoc = FTL.getRParenLoc();
            if(TypeHintLoc.isValid())
                add_return_type_hint(D, TypeHintLoc);
        }
        return true;
    }

    bool VisitInitListExpr(InitListExpr* expr) {
        // We receive the syntactic form here (shouldVisitImplicitCode() is false).
        // This is the one we will ultimately attach designators to.
        // It may have subobject initializers inlined without braces. The *semantic*
        // form of the init-list has nested init-lists for these.
        // getUnwrittenDesignators will look at the semantic form to determine the
        // labels.
        assert(expr->isSyntacticForm() && "RAV should not visit implicit code!");
        if(!options.designators)
            return true;

        if(expr->isIdiomaticZeroInitializer(AST.getLangOpts()))
            return true;

        /// FIXME:
        // llvm::DenseMap<SourceLocation, std::string> Designators =
        //     tidy::utils::getUnwrittenDesignators(Syn);
        // for(const Expr* Init: Syn->inits()) {
        //     if(llvm::isa<DesignatedInitExpr>(Init))
        //         continue;
        //     auto It = Designators.find(Init->getBeginLoc());
        //     if(It != Designators.end() && !isPrecededByParamNameComment(Init, It->second))
        //         addDesignatorHint(Init->getSourceRange(), It->second);
        // }
        return true;
    }

    bool VisitTypeLoc(TypeLoc TL) {
        if(const auto* DT = llvm::dyn_cast<DecltypeType>(TL.getType()))
            if(QualType UT = DT->getUnderlyingType(); !UT->isDependentType())
                add_type_hint(TL.getSourceRange(), UT, ": ");
        return true;
    }

    // FIXME: Handle RecoveryExpr to try to hint some invalid calls.

private:
    std::vector<InlayHint>& results;
    CompilationUnit& unit;
    ASTContext& AST;
    const syntax::TokenBuffer& Tokens;
    LocalSourceRange restrict_range;
    FileID MainFileID;
    StringRef MainFileBuf;
    PrintingPolicy TypeHintPolicy;
    config::InlayHintsOptions options;

    // If/else chains are tricky.
    //   if (cond1) {
    //   } else if (cond2) {
    //   } // mark as "cond1" or "cond2"?
    // For now, the answer is neither, just mark as "if".
    // The ElseIf is a different IfStmt that doesn't know about the outer one.
    llvm::DenseSet<const IfStmt*> ElseIfs;  // not eligible for names
};

}  // namespace

InlayHints inlay_hints(CompilationUnit& unit, LocalSourceRange target) {
    std::vector<InlayHint> hints;
    InlayHintVisitor visitor(hints, unit, target);
    visitor.TraverseDecl(unit.tu());
    return hints;
}

}  // namespace clice::feature
