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

bool isExpandedFromParameterPack(const ParmVarDecl* D) {
    return ast::getUnderlyingPackType(D) != nullptr;
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
            return ast::identifier_of(*DRE->getDecl());

    if(auto* ME = dyn_cast<MemberExpr>(E))
        if(!ME->getQualifier() && ME->isImplicitAccess())
            return ast::identifier_of(*ME->getMemberDecl());

    return {};
}

using NameVec = SmallVector<StringRef, 8>;

static bool isSetter(const FunctionDecl* callee, const NameVec& ParamNames) {
    if(ParamNames.size() != 1)
        return false;

    StringRef Name = ast::identifier_of(*callee);
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
    const FunctionDecl* decl = nullptr;
    FunctionProtoTypeLoc loc;
};

class Builder {
public:
    Builder(std::vector<InlayHint>& result,
            CompilationUnit& unit,
            LocalSourceRange restrict_range,
            const config::InlayHintsOptions& options) :
        result(result), unit(unit), restrict_range(restrict_range), options(options),
        policy(unit.context().getPrintingPolicy()) {
        // The sugared type is more useful in some cases, and the canonical
        // type in other cases.
        policy.SuppressScope = true;           // keep type names short
        policy.AnonymousTagLocations = false;  // do not print lambda locations
        // Not setting PrintCanonicalTypes for "auto" allows
        // SuppressDefaultTemplateArgs (set by default) to have an effect.
    }

private:
    // Get the range of the main file that *exactly* corresponds to R.
    std::optional<LocalSourceRange> getHintRange(SourceRange R) {
        const auto& SM = unit.context().getSourceManager();
        auto Spelled = unit.token_buffer().spelledForExpanded(unit.expanded_tokens(R));
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

        auto [fid, begin] = unit.decompose_location(Spelled->front().location());
        auto [fid2, end] = unit.decompose_location(Spelled->back().endLocation());

        return LocalSourceRange{begin, end};
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

        auto& SM = unit.context().getSourceManager();
        auto [BlockBeginFileId, BlockBeginOffset] =
            SM.getDecomposedLoc(SM.getFileLoc(BraceRange.getBegin()));
        auto RBraceLoc = SM.getFileLoc(BraceRange.getEnd());
        auto [RBraceFileId, RBraceOffset] = SM.getDecomposedLoc(RBraceLoc);

        // Because we need to check the block satisfies the minimum line limit, we
        // require both source location to be in the main file. This prevents hint
        // to be shown in weird cases like '{' is actually in a "#include", but it's
        // rare anyway.
        if(BlockBeginFileId != unit.interested_file() || RBraceFileId != unit.interested_file())
            return std::nullopt;

        StringRef RestOfLine = unit.interested_content().substr(RBraceOffset).split('\n').first;
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

    /// Check whether the expr has a param name comment before it.
    /// The typical format is `/*name=*/`.
    bool has_param_name_comment(const Expr* expr, StringRef name) {
        auto location = unit.file_location(expr->getBeginLoc());
        auto [fid, offset] = unit.decompose_location(location);
        if(fid != unit.interested_file()) {
            return false;
        }

        StringRef content = unit.interested_content().substr(0, offset);

        // Allow whitespace between comment and expression.
        content = content.rtrim();
        if(!content.consume_back("*/")) {
            return false;
        }

        // Ignore some punctuation and whitespace around comment.
        // In particular this allows designators to match nicely.
        llvm::StringLiteral ignore_chars = " =.";
        name = name.trim(ignore_chars);
        content = content.rtrim(ignore_chars);

        // Other than that, the comment must contain exactly name.
        if(!content.consume_back(name)) {
            return false;
        }

        content = content.rtrim(ignore_chars);
        return content.ends_with("/*");
    }

    bool shouldHintName(const Expr* Arg, StringRef ParamName) {
        if(ParamName.empty())
            return false;

        // If the argument expression is a single name and it matches the
        // parameter name exactly, omit the name hint.
        if(ParamName == getSpelledIdentifier(Arg))
            return false;

        // Exclude argument expressions preceded by a /*paramName*/.
        if(has_param_name_comment(Arg, ParamName))
            return false;

        return true;
    }

    bool shouldHintReference(const ParmVarDecl* param, const ParmVarDecl* forwarded_param) {
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
        auto type = param->getType();
        auto forwarded_type = forwarded_param->getType();
        return type->isLValueReferenceType() && forwarded_type->isLValueReferenceType() &&
               !forwarded_type.getNonReferenceType().isConstQualified() &&
               !isExpandedFromParameterPack(forwarded_param);
    }

    using NameVec = SmallVector<StringRef, 8>;

    NameVec chooseParameterNames(ArrayRef<const ParmVarDecl*> Parameters) {
        NameVec ParameterNames;
        for(const auto* P: Parameters) {
            if(isExpandedFromParameterPack(P)) {
                // If we haven't resolved a pack paramater (e.g. foo(Args... args)) to a
                // non-pack parameter, then hinting as foo(args: 1, args: 2, args: 3) is
                // unlikely to be useful.
                ParameterNames.emplace_back();
            } else {
                auto SimpleName = ast::identifier_of(*P);
                // If the parameter is unnamed in the declaration:
                // attempt to get its name from the definition
                if(SimpleName.empty()) {
                    if(const auto* PD = getParamDefinition(P)) {
                        SimpleName = ast::identifier_of(*PD);
                    }
                }
                ParameterNames.emplace_back(SimpleName);
            }
        }

        // Standard library functions often have parameter names that start
        // with underscores, which makes the hints noisy, so strip them out.
        for(auto& Name: ParameterNames) {
            Name = Name.ltrim('_');
        }

        return ParameterNames;
    }

public:
    void add_params(Callee callee,
                    SourceLocation RParenOrBraceLoc,
                    llvm::ArrayRef<const Expr*> args) {
        assert(callee.decl || callee.loc);

        if((!options.parameters && !options.default_arguments) || args.size() == 0) {
            return;
        }

        if(callee.decl) {
            /// We don't want to hint for copy or move constructors, which may make
            /// a lot of noise.
            auto ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(callee.decl);
            if(ctor && ctor->isCopyOrMoveConstructor()) {
                return;
            }
        }

        SmallVector<std::string> FormattedDefaultArgs;
        bool HasNonDefaultArgs = false;

        ArrayRef<const ParmVarDecl*> Params, ForwardedParams;
        // Resolve parameter packs to their forwarded parameter
        SmallVector<const ParmVarDecl*> ForwardedParamsStorage;

        auto maybeDropCxxExplicitObjectParameters =
            [](ArrayRef<const ParmVarDecl*> params) -> ArrayRef<const ParmVarDecl*> {
            if(!params.empty() && params.front()->isExplicitObjectParameter()) {
                params = params.drop_front(1);
            }
            return params;
        };

        if(callee.decl) {
            Params = maybeDropCxxExplicitObjectParameters(callee.decl->parameters());
            ForwardedParamsStorage = ast::resolveForwardingParameters(callee.decl);
            ForwardedParams = maybeDropCxxExplicitObjectParameters(ForwardedParamsStorage);
        } else {
            Params = maybeDropCxxExplicitObjectParameters(callee.loc.getParams());
            ForwardedParams = {Params.begin(), Params.end()};
        }

        NameVec param_names = chooseParameterNames(ForwardedParams);

        // Exclude setters (i.e. functions with one argument whose name begins with
        // "set"), and builtins like std::move/forward/... as their parameter name
        // is also not likely to be interesting.
        if(callee.decl && (isSetter(callee.decl, param_names) || isSimpleBuiltin(callee.decl)))
            return;

        for(size_t i = 0; i < param_names.size() && i < args.size(); ++i) {
            // Pack expansion expressions cause the 1:1 mapping between arguments and
            // parameters to break down, so we don't add further inlay hints if we
            // encounter one.
            if(llvm::isa<clang::PackExpansionExpr>(args[i])) {
                break;
            }

            StringRef Name = param_names[i];
            const bool NameHint = shouldHintName(args[i], Name) && options.parameters;
            const bool ReferenceHint =
                shouldHintReference(Params[i], ForwardedParams[i]) && options.parameters;

            const bool IsDefault = isa<CXXDefaultArgExpr>(args[i]);
            HasNonDefaultArgs |= !IsDefault;
            if(IsDefault) {
                if(options.default_arguments) {
                    const auto SourceText = clang::Lexer::getSourceText(
                        CharSourceRange::getTokenRange(Params[i]->getDefaultArgRange()),
                        unit.context().getSourceManager(),
                        unit.lang_options());
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
                add_inlay_hint(args[i]->getSourceRange(),
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

    void add_block_end_hint(SourceRange BraceRange,
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

    void mark_block_end(const Stmt* Body, llvm::StringRef Label, llvm::StringRef Name = "") {
        if(const auto* CS = llvm::dyn_cast_or_null<CompoundStmt>(Body)) {
            add_block_end_hint(CS->getSourceRange(), Label, Name, "");
        }
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
        result.push_back(std::move(hint));
    }

    void add_type_hint(SourceRange R, QualType T, llvm::StringRef Prefix) {
        if(!options.deduced_types || T.isNull())
            return;

        auto Desugared = ast::maybeDesugar(unit.context(), T);
        std::string TypeName = Desugared.getAsString(policy);

        auto should_print = [&](llvm::StringRef TypeName) {
            return options.type_name_limit == 0 || TypeName.size() < options.type_name_limit;
        };

        if(T != Desugared && !should_print(TypeName)) {
            // If the desugared type is too long to display, fallback to the sugared
            // type.
            TypeName = T.getAsString(policy);
        }

        if(should_print(TypeName))
            add_inlay_hint(R,
                           HintSide::Right,
                           InlayHintKind::Type,
                           Prefix,
                           TypeName,
                           /*Suffix=*/"");
    }

    void add_designator_hint(SourceRange R, llvm::StringRef Text) {
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

private:
    std::vector<InlayHint>& result;
    CompilationUnit& unit;
    LocalSourceRange restrict_range;
    const config::InlayHintsOptions& options;
    clang::PrintingPolicy policy;
};

class InlayHintVisitor : public FilteredASTVisitor<InlayHintVisitor> {
public:
    using Base = FilteredASTVisitor<InlayHintVisitor>;

    InlayHintVisitor(Builder& builder,
                     CompilationUnit& unit,
                     std::optional<LocalSourceRange> restrict_range) :
        Base(unit, true), builder(builder), unit(unit), AST(unit.context()) {}

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
            builder.add_block_end_hint(D->getSourceRange(),
                                       "namespace",
                                       ast::identifier_of(*D),
                                       "");
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
            builder.add_block_end_hint(D->getBraceRange(), DeclPrefix, ast::identifier_of(*D), ";");
        }
        return true;
    }

    bool VisitFunctionDecl(FunctionDecl* D) {
        if(auto* FPT = llvm::dyn_cast<FunctionProtoType>(D->getType().getTypePtr())) {
            if(!FPT->hasTrailingReturn()) {
                if(auto FTL = D->getFunctionTypeLoc())
                    builder.add_return_type_hint(D, FTL.getRParenLoc());
            }
        }

        if(options.block_end && D->isThisDeclarationADefinition()) {
            // We use `printName` here to properly print name of ctor/dtor/operator
            // overload.
            if(const Stmt* Body = D->getBody())
                builder.add_block_end_hint(Body->getSourceRange(), "", ast::print_name(D), "");
        }

        return true;
    }

    bool VisitVarDecl(VarDecl* var) {
        // Do not show hints for the aggregate in a structured binding,
        // but show hints for the individual bindings.
        if(auto* decl = dyn_cast<DecompositionDecl>(var)) {
            for(auto* binding: decl->bindings()) {
                // For structured bindings, print canonical types. This is important
                // because for bindings that use the tuple_element protocol, the
                // non-canonical types would be "tuple_element<I, A>::type".
                if(auto type = binding->getType(); !type.isNull() && !type->isDependentType()) {
                    builder.add_type_hint(binding->getLocation(),
                                          type.getCanonicalType(),
                                          /*Prefix=*/": ");
                }
            }
            return true;
        }

        auto type = var->getType();
        if(auto* auto_type = type->getContainedAutoType()) {
            if(auto_type->isDeduced() && !type->isDependentType()) {
                // Our current approach is to place the hint on the variable
                // and accordingly print the full type
                // (e.g. for `const auto& x = 42`, print `const int&`).
                // Alternatively, we could place the hint on the `auto`
                // (and then just print the type deduced for the `auto`).
                builder.add_type_hint(var->getLocation(), var->getType(), /*Prefix=*/": ");
            }
        }

        // Handle templates like `int foo(auto x)` with exactly one instantiation.
        if(auto* param = llvm::dyn_cast<clang::ParmVarDecl>(var)) {
            if(var->getIdentifier() && type->isDependentType()) {
                auto unwrapped = ast::unwrap_type(var->getTypeSourceInfo()->getTypeLoc());
                if(auto type = unwrapped.getAs<clang::TemplateTypeParmTypeLoc>()) {
                    if(auto decl = type.getDecl(); decl && decl->isImplicit()) {
                        if(auto* IPVD = ast::get_only_instantiation(param)) {
                            builder.add_type_hint(var->getLocation(),
                                                  IPVD->getType(),
                                                  /*Prefix=*/": ");
                        }
                    }
                }
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

        Callee callee;
        callee.decl = E->getConstructor();
        if(!callee.decl) {
            return true;
        }

        builder.add_params(callee,
                           E->getParenOrBraceRange().getEnd(),
                           {E->getArgs(), E->getNumArgs()});
        return true;
    }

    bool VisitCallExpr(CallExpr* E) {
        if(!options.parameters) {
            return true;
        }

        auto isFunctionObjectCallExpr = [](CallExpr* E) {
            if(auto* CallExpr = dyn_cast<CXXOperatorCallExpr>(E)) {
                return CallExpr->getOperator() == OverloadedOperatorKind::OO_Call;
            }

            return false;
        };

        bool is_functor = isFunctionObjectCallExpr(E);

        // Do not show parameter hints for user-defined literals or
        // operator calls except for operator(). (Among other reasons, the resulting
        // hints can look awkward, e.g. the expression can itself be a function
        // argument and then we'd get two hints side by side).
        if((isa<CXXOperatorCallExpr>(E) && !is_functor) || isa<UserDefinedLiteral>(E))
            return true;

        /// FIXME: Use template resolver here.
        if(E->isTypeDependent() || E->isValueDependent()) {
            return true;
        }

        auto callee_decl = E->getCalleeDecl();

        Callee callee;
        if(const auto* FD = dyn_cast<FunctionDecl>(callee_decl)) {
            callee.decl = FD;
        } else if(const auto* FTD = dyn_cast<FunctionTemplateDecl>(callee_decl)) {
            callee.decl = FTD->getTemplatedDecl();
        } else if(FunctionProtoTypeLoc loc = ast::getPrototypeLoc(E->getCallee())) {
            callee.loc = loc;
        } else {
            return true;
        }

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
        llvm::ArrayRef<const Expr*> args = {E->getArgs(), E->getNumArgs()};

        // We don't have the implied object argument through a function pointer either.
        if(const auto* method = dyn_cast_or_null<CXXMethodDecl>(callee.decl)) {
            if(is_functor || method->hasCXXExplicitFunctionObjectParameter()) {
                args = args.drop_front(1);
            }
        }

        builder.add_params(callee, E->getRParenLoc(), args);
        return true;
    }

    bool VisitForStmt(ForStmt* S) {
        if(options.block_end) {
            std::string name;
            // Common case: for (int I = 0; I < N; I++). Use "I" as the name.
            if(auto* DS = llvm::dyn_cast_or_null<DeclStmt>(S->getInit());
               DS && DS->isSingleDecl()) {
                name = ast::identifier_of(llvm::cast<NamedDecl>(*DS->getSingleDecl()));
            } else {
                name = ast::summarize_expr(S->getCond());
            }
            builder.mark_block_end(S->getBody(), "for", name);
        }
        return true;
    }

    bool VisitCXXForRangeStmt(CXXForRangeStmt* S) {
        if(options.block_end) {
            builder.mark_block_end(S->getBody(), "for", ast::identifier_of(*S->getLoopVariable()));
        }
        return true;
    }

    bool VisitWhileStmt(WhileStmt* S) {
        if(options.block_end)
            builder.mark_block_end(S->getBody(), "while", ast::summarize_expr(S->getCond()));
        return true;
    }

    bool VisitSwitchStmt(SwitchStmt* S) {
        if(options.block_end)
            builder.mark_block_end(S->getBody(), "switch", ast::summarize_expr(S->getCond()));
        return true;
    }

    bool VisitIfStmt(IfStmt* S) {
        if(options.block_end) {
            if(const auto* ElseIf = llvm::dyn_cast_or_null<IfStmt>(S->getElse())) {
                else_ifs.insert(ElseIf);
            }

            // Don't use markBlockEnd: the relevant range is [then.begin, else.end].
            if(const auto* EndCS =
                   llvm::dyn_cast<CompoundStmt>(S->getElse() ? S->getElse() : S->getThen())) {
                builder.add_block_end_hint({S->getThen()->getBeginLoc(), EndCS->getRBracLoc()},
                                           "if",
                                           else_ifs.contains(S) ? ""
                                                                : ast::summarize_expr(S->getCond()),
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
                builder.add_return_type_hint(D, TypeHintLoc);
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
                builder.add_type_hint(TL.getSourceRange(), UT, ": ");
        return true;
    }

    // FIXME: Handle RecoveryExpr to try to hint some invalid calls.

private:
    Builder& builder;
    CompilationUnit& unit;
    ASTContext& AST;
    config::InlayHintsOptions options;

    // If/else chains are tricky.
    //   if (cond1) {
    //   } else if (cond2) {
    //   } // mark as "cond1" or "cond2"?
    // For now, the answer is neither, just mark as "if".
    // The ElseIf is a different IfStmt that doesn't know about the outer one.
    llvm::DenseSet<const IfStmt*> else_ifs;  // not eligible for names
};

}  // namespace

InlayHints inlay_hints(CompilationUnit& unit, LocalSourceRange target) {
    std::vector<InlayHint> hints;
    config::InlayHintsOptions options;
    Builder builder(hints, unit, target, options);
    InlayHintVisitor visitor(builder, unit, target);
    visitor.TraverseDecl(unit.tu());

    ranges::sort(hints, refl::less);
    auto sub_range = ranges::unique(hints, refl::equal);
    hints.erase(sub_range.begin(), sub_range.end());

    return hints;
}

}  // namespace clice::feature
