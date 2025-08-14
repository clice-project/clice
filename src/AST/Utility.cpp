#include "AST/Utility.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/ScopedPrinter.h"
#include "llvm/ADT/SmallSet.h"
#include "clang/AST/ASTDiagnostic.h"

namespace clice::ast {

bool is_definition(const clang::Decl* decl) {
    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return VD->isThisDeclarationADefinition();
    }

    if(auto FD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        return FD->isThisDeclarationADefinition();
    }

    if(auto TD = llvm::dyn_cast<clang::TagDecl>(decl)) {
        return TD->isThisDeclarationADefinition();
    }

    if(llvm::isa<clang::FieldDecl,
                 clang::EnumConstantDecl,
                 clang::TypedefNameDecl,
                 clang::ConceptDecl>(decl)) {
        return true;
    }

    return false;
}

bool is_templated(const clang::Decl* decl) {
    if(decl->getDescribedTemplate()) {
        return true;
    }

    if(llvm::isa<clang::TemplateDecl,
                 clang::ClassTemplatePartialSpecializationDecl,
                 clang::VarTemplatePartialSpecializationDecl>(decl)) {
        return true;
    }

    return false;
}

const static clang::CXXRecordDecl* getDeclContextForTemplateInstationPattern(const clang::Decl* D) {
    if(const auto* CTSD = dyn_cast<clang::ClassTemplateSpecializationDecl>(D->getDeclContext())) {
        return CTSD->getTemplateInstantiationPattern();
    }

    if(const auto* RD = dyn_cast<clang::CXXRecordDecl>(D->getDeclContext())) {
        return RD->getInstantiatedFromMemberClass();
    }

    return nullptr;
}

const clang::NamedDecl* instantiated_from(const clang::NamedDecl* decl) {
    if(auto CTSD = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {

        auto kind = CTSD->getTemplateSpecializationKind();
        if(kind == clang::TSK_Undeclared) {
            /// The instantiation of template is lazy, in this case, the specialization is
            /// undeclared. Temporarily return primary template of the specialization.
            /// FIXME: Is there a better way to handle such case?
            return CTSD->getSpecializedTemplate()->getTemplatedDecl();
        } else if(kind == clang::TSK_ExplicitSpecialization) {
            /// If the decl is an full specialization, return itself.
            return CTSD;
        }

        return CTSD->getTemplateInstantiationPattern();
    }

    if(auto FD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        /// If the decl is an full specialization, return itself.
        if(FD->getTemplateSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            return FD;
        }

        return FD->getTemplateInstantiationPattern();
    }

    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        /// If the decl is an full specialization, return itself.
        if(VD->getTemplateSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            return VD;
        }

        return VD->getTemplateInstantiationPattern();
    }

    if(auto CRD = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        return CRD->getInstantiatedFromMemberClass();
    }

    /// For `FieldDecl` and `TypedefNameDecl`, clang will not store their instantiation information
    /// in the unit. So we need to look up the original decl manually.
    if(llvm::isa<clang::FieldDecl, clang::TypedefNameDecl>(decl)) {
        /// FIXME: figure out the context.
        if(auto context = getDeclContextForTemplateInstationPattern(decl)) {
            for(auto member: context->lookup(decl->getDeclName())) {
                if(member->isImplicit()) {
                    continue;
                }

                if(member->getKind() == decl->getKind()) {
                    return member;
                }
            }
        }
    }

    if(auto ED = llvm::dyn_cast<clang::EnumDecl>(decl)) {
        return ED->getInstantiatedFromMemberEnum();
    }

    if(auto ECD = llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
        auto ED = llvm::cast<clang::EnumDecl>(ECD->getDeclContext());
        if(auto context = ED->getInstantiatedFromMemberEnum()) {
            for(auto member: context->lookup(ECD->getDeclName())) {
                return member;
            }
        }
    }

    return nullptr;
}

const clang::NamedDecl* normalize(const clang::NamedDecl* decl) {
    if(!decl) {
        std::abort();
    }

    decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());

    if(auto ND = instantiated_from(llvm::cast<clang::NamedDecl>(decl))) {
        return llvm::cast<clang::NamedDecl>(ND->getCanonicalDecl());
    }

    return decl;
}

std::string name_of(const clang::NamedDecl* decl) {
    llvm::SmallString<128> result;

    auto name = decl->getDeclName();
    switch(name.getNameKind()) {
        case clang::DeclarationName::Identifier: {
            if(auto II = name.getAsIdentifierInfo()) {
                result += name.getAsIdentifierInfo()->getName();
            }
            break;
        }

        case clang::DeclarationName::CXXConstructorName: {
            result += name.getCXXNameType().getAsString();
            break;
        }

        case clang::DeclarationName::CXXDestructorName: {
            result += '~';
            result += name.getCXXNameType().getAsString();
            break;
        }

        case clang::DeclarationName::CXXConversionFunctionName: {
            result += "operator ";
            result += name.getCXXNameType().getAsString();
            break;
        }

        case clang::DeclarationName::CXXOperatorName: {
            result += "operator ";
            result += clang::getOperatorSpelling(name.getCXXOverloadedOperator());
            break;
        }

        case clang::DeclarationName::CXXDeductionGuideName: {
            result += name.getCXXDeductionGuideTemplate()->getNameAsString();
            break;
        }

        case clang::DeclarationName::CXXLiteralOperatorName: {
            result += R"(operator "")";
            result += name.getCXXLiteralIdentifier()->getName();
            break;
        }

        case clang::DeclarationName::CXXUsingDirective: {
            auto UDD = llvm::cast<clang::UsingDirectiveDecl>(decl);
            result += UDD->getNominatedNamespace()->getName();
            break;
        }

        case clang::DeclarationName::ObjCZeroArgSelector:
        case clang::DeclarationName::ObjCOneArgSelector:
        case clang::DeclarationName::ObjCMultiArgSelector: {
            std::unreachable();
        }
    }

    return result.str().str();
}

clang::QualType type_of(const clang::NamedDecl* decl) {
    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return VD->getType();
    }

    if(auto FD = llvm::dyn_cast<clang::FieldDecl>(decl)) {
        return FD->getType();
    }

    if(auto ECD = llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
        return ECD->getType();
    }

    if(auto BD = llvm::dyn_cast<clang::BindingDecl>(decl)) {
        return BD->getType();
    }

    if(auto TD = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
        return TD->getUnderlyingType();
    }

    if(auto CCD = llvm::dyn_cast<clang::CXXConstructorDecl>(decl)) {
        return CCD->getThisType();
    }

    if(auto CDD = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
        return CDD->getThisType();
    }

    return clang::QualType();
}

// getDeclForType() returns the decl responsible for Type's spelling.
// This is the inverse of ASTContext::getTypeDeclType().
template <typename Ty>
    requires requires(Ty* T) { T->getDecl(); }
const clang::NamedDecl* get_decl_for_type_impl(const Ty* T) {
    return T->getDecl();
}

const clang::NamedDecl* get_decl_for_type_impl(const void* T) {
    return nullptr;
}

const clang::NamedDecl* get_decl_for_type(const clang::Type* T) {
    switch(T->getTypeClass()) {
#define ABSTRACT_TYPE(TY, BASE)
#define TYPE(TY, BASE)                                                                             \
    case clang::Type::TY: return get_decl_for_type_impl(llvm::cast<clang::TY##Type>(T));
#include "clang/AST/TypeNodes.inc"
    }
    llvm_unreachable("Unknown TypeClass enum");
}

const clang::NamedDecl* decl_of(clang::QualType type) {
    if(type.isNull()) {
        return nullptr;
    }

    if(auto RT = type->getAs<clang::TagType>()) {
        return RT->getDecl();
    }

    if(auto TT = type->getAs<clang::TagType>()) {
        return TT->getDecl();
    }

    /// FIXME:
    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {

        auto decl = TST->getTemplateName().getAsTemplateDecl();
        if(type->isDependentType()) {
            return decl;
        }

        /// For a template specialization type, the template name is possibly a `ClassTemplateDecl`
        ///  `TypeAliasTemplateDecl` or `TemplateTemplateParmDecl` and `BuiltinTemplateDecl`.
        if(llvm::isa<clang::TypeAliasTemplateDecl>(decl)) {
            return decl->getTemplatedDecl();
        }

        if(llvm::isa<clang::TemplateTemplateParmDecl, clang::BuiltinTemplateDecl>(decl)) {
            return decl;
        }

        return instantiated_from(TST->getAsCXXRecordDecl());
    }

    return nullptr;
}

bool is_anonymous(const clang::NamedDecl* decl) {
    auto name = decl->getDeclName();
    return name.isIdentifier() && !name.getAsIdentifierInfo();
}

clang::NestedNameSpecifierLoc get_qualifier_loc(const clang::NamedDecl* decl) {
    if(auto* V = llvm::dyn_cast<clang::DeclaratorDecl>(decl)) {
        return V->getQualifierLoc();
    }

    if(auto* T = llvm::dyn_cast<clang::TagDecl>(decl)) {
        return T->getQualifierLoc();
    }

    return clang::NestedNameSpecifierLoc();
}

auto get_template_specialization_args(const clang::NamedDecl* decl)
    -> std::optional<llvm::ArrayRef<clang::TemplateArgumentLoc>> {
    if(auto* FD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        if(const clang::ASTTemplateArgumentListInfo* Args =
               FD->getTemplateSpecializationArgsAsWritten()) {
            return Args->arguments();
        }
    } else if(auto* Cls = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
        if(auto* Args = Cls->getTemplateArgsAsWritten()) {
            return Args->arguments();
        }
    } else if(auto* Var = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
        if(auto* Args = Var->getTemplateArgsAsWritten()) {
            return Args->arguments();
        }
    }

    // We return std::nullopt for ClassTemplateSpecializationDecls because it does
    // not contain TemplateArgumentLoc information.
    return std::nullopt;
}

std::string print_template_specialization_args(const clang::NamedDecl* decl) {
    std::string TemplateArgs;
    llvm::raw_string_ostream OS(TemplateArgs);
    clang::PrintingPolicy Policy(decl->getASTContext().getLangOpts());
    if(auto Args = ast::get_template_specialization_args(decl)) {
        printTemplateArgumentList(OS, *Args, Policy);
    } else if(auto* Cls = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
        // FIXME: Fix cases when getTypeAsWritten returns null inside clang AST,
        // e.g. friend decls. Currently we fallback to Template Arguments without
        // location information.
        printTemplateArgumentList(OS, Cls->getTemplateArgs().asArray(), Policy);
    }
    return TemplateArgs;
}

std::string print_name(const clang::NamedDecl* decl) {
    std::string Name;
    llvm::raw_string_ostream Out(Name);
    clang::PrintingPolicy PP(decl->getASTContext().getLangOpts());
    // We don't consider a class template's args part of the constructor name.
    PP.SuppressTemplateArgsInCXXConstructors = true;

    // Handle 'using namespace'. They all have the same name - <using-directive>.
    if(auto* UD = llvm::dyn_cast<clang::UsingDirectiveDecl>(decl)) {
        Out << "using namespace ";
        if(auto* Qual = UD->getQualifier())
            Qual->print(Out, PP);
        UD->getNominatedNamespaceAsWritten()->printName(Out);
        return Out.str();
    }

    if(ast::is_anonymous(decl)) {
        // Come up with a presentation for an anonymous entity.
        if(isa<clang::NamespaceDecl>(decl))
            return "(anonymous namespace)";
        if(auto* Cls = llvm::dyn_cast<clang::RecordDecl>(decl)) {
            if(Cls->isLambda())
                return "(lambda)";
            return ("(anonymous " + Cls->getKindName() + ")").str();
        }
        if(isa<clang::EnumDecl>(decl))
            return "(anonymous enum)";
        return "(anonymous)";
    }

    // Print nested name qualifier if it was written in the source code.
    if(auto* Qualifier = ast::get_qualifier_loc(decl).getNestedNameSpecifier())
        Qualifier->print(Out, PP);
    // Print the name itself.
    decl->getDeclName().print(Out, PP);
    // Print template arguments.
    Out << ast::print_template_specialization_args(decl);

    return Out.str();
}

clang::TemplateTypeParmTypeLoc get_contained_auto_param_type(clang::TypeLoc TL) {
    if(auto QTL = TL.getAs<clang::QualifiedTypeLoc>()) {
        return get_contained_auto_param_type(QTL.getUnqualifiedLoc());
    }

    if(llvm::isa<clang::PointerType, clang::ReferenceType, clang::ParenType>(TL.getTypePtr())) {
        return get_contained_auto_param_type(TL.getNextTypeLoc());
    }

    if(auto FTL = TL.getAs<clang::FunctionTypeLoc>()) {
        return get_contained_auto_param_type(FTL.getReturnLoc());
    }

    if(auto TTPTL = TL.getAs<clang::TemplateTypeParmTypeLoc>()) {
        if(TTPTL.getTypePtr()->getDecl()->isImplicit()) {
            return TTPTL;
        }
    }

    return {};
}

template <typename TemplateDeclTy>
static clang::NamedDecl* get_only_instantiation_impl(TemplateDeclTy* TD) {
    clang::NamedDecl* Only = nullptr;
    for(auto* Spec: TD->specializations()) {
        if(Spec->getTemplateSpecializationKind() == clang::TSK_ExplicitSpecialization)
            continue;
        if(Only != nullptr)
            return nullptr;
        Only = Spec;
    }
    return Only;
}

clang::NamedDecl* get_only_instantiation(clang::NamedDecl* TemplatedDecl) {
    if(auto* TD = TemplatedDecl->getDescribedTemplate()) {
        if(auto* CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD))
            return get_only_instantiation_impl(CTD);
        if(auto* FTD = llvm::dyn_cast<clang::FunctionTemplateDecl>(TD))
            return get_only_instantiation_impl(FTD);
        if(auto* VTD = llvm::dyn_cast<clang::VarTemplateDecl>(TD))
            return get_only_instantiation_impl(VTD);
    }
    return nullptr;
}

// getSimpleName() returns the plain identifier for an entity, if any.
llvm::StringRef getSimpleName(const clang::DeclarationName& DN) {
    if(clang::IdentifierInfo* Ident = DN.getAsIdentifierInfo())
        return Ident->getName();
    return "";
}

llvm::StringRef getSimpleName(const clang::NamedDecl& D) {
    return getSimpleName(D.getDeclName());
}

llvm::StringRef getSimpleName(clang::QualType T) {
    if(const auto* ET = llvm::dyn_cast<clang::ElaboratedType>(T))
        return getSimpleName(ET->getNamedType());
    if(const auto* BT = llvm::dyn_cast<clang::BuiltinType>(T)) {
        clang::PrintingPolicy PP(clang::LangOptions{});
        PP.adjustForCPlusPlus();
        return BT->getName(PP);
    }
    if(const auto* D = ast::get_decl_for_type(T.getTypePtr()))
        return getSimpleName(D->getDeclName());
    return "";
}

std::string summarizeExpr(const clang::Expr* E) {
    using namespace clang;

    struct Namer : ConstStmtVisitor<Namer, std::string> {
        std::string Visit(const Expr* E) {
            if(E == nullptr)
                return "";
            return ConstStmtVisitor::Visit(E->IgnoreImplicit());
        }

        // Any sort of decl reference, we just use the unqualified name.
        std::string VisitMemberExpr(const MemberExpr* E) {
            return ast::getSimpleName(*E->getMemberDecl()).str();
        }

        std::string VisitDeclRefExpr(const DeclRefExpr* E) {
            return ast::getSimpleName(*E->getFoundDecl()).str();
        }

        std::string VisitCallExpr(const CallExpr* E) {
            return Visit(E->getCallee());
        }

        std::string VisitCXXDependentScopeMemberExpr(const CXXDependentScopeMemberExpr* E) {
            return ast::getSimpleName(E->getMember()).str();
        }

        std::string VisitDependentScopeDeclRefExpr(const DependentScopeDeclRefExpr* E) {
            return ast::getSimpleName(E->getDeclName()).str();
        }

        std::string VisitCXXFunctionalCastExpr(const CXXFunctionalCastExpr* E) {
            return ast::getSimpleName(E->getType()).str();
        }

        std::string VisitCXXTemporaryObjectExpr(const CXXTemporaryObjectExpr* E) {
            return ast::getSimpleName(E->getType()).str();
        }

        // Step through implicit nodes that clang doesn't classify as such.
        std::string VisitCXXMemberCallExpr(const CXXMemberCallExpr* E) {
            // Call to operator bool() inside if (X): dispatch to X.
            if(E->getNumArgs() == 0 && E->getMethodDecl() &&
               E->getMethodDecl()->getDeclName().getNameKind() ==
                   DeclarationName::CXXConversionFunctionName &&
               E->getSourceRange() == E->getImplicitObjectArgument()->getSourceRange())
                return Visit(E->getImplicitObjectArgument());
            return ConstStmtVisitor::VisitCXXMemberCallExpr(E);
        }

        std::string VisitCXXConstructExpr(const CXXConstructExpr* E) {
            if(E->getNumArgs() == 1)
                return Visit(E->getArg(0));
            return "";
        }

        // Literals are just printed
        std::string VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr* E) {
            return E->getValue() ? "true" : "false";
        }

        std::string VisitIntegerLiteral(const IntegerLiteral* E) {
            return llvm::to_string(E->getValue());
        }

        std::string VisitFloatingLiteral(const FloatingLiteral* E) {
            std::string Result;
            llvm::raw_string_ostream OS(Result);
            E->getValue().print(OS);
            // Printer adds newlines?!
            Result.resize(llvm::StringRef(Result).rtrim().size());
            return Result;
        }

        std::string VisitStringLiteral(const StringLiteral* E) {
            std::string Result = "\"";
            if(E->containsNonAscii()) {
                Result += "...";
            } else if(E->getLength() > 10) {
                Result += E->getString().take_front(7);
                Result += "...";
            } else {
                llvm::raw_string_ostream OS(Result);
                llvm::printEscapedString(E->getString(), OS);
            }
            Result.push_back('"');
            return Result;
        }

        // Simple operators. Motivating cases are `!x` and `I < Length`.
        std::string printUnary(llvm::StringRef Spelling, const Expr* Operand, bool Prefix) {
            std::string Sub = Visit(Operand);
            if(Sub.empty())
                return "";
            if(Prefix)
                return (Spelling + Sub).str();
            Sub += Spelling;
            return Sub;
        }

        bool InsideBinary = false;  // No recursing into binary expressions.

        std::string printBinary(llvm::StringRef Spelling, const Expr* LHSOp, const Expr* RHSOp) {
            if(InsideBinary)
                return "";
            llvm::SaveAndRestore InBinary(InsideBinary, true);

            std::string LHS = Visit(LHSOp);
            std::string RHS = Visit(RHSOp);
            if(LHS.empty() && RHS.empty())
                return "";

            if(LHS.empty())
                LHS = "...";
            LHS.push_back(' ');
            LHS += Spelling;
            LHS.push_back(' ');
            if(RHS.empty())
                LHS += "...";
            else
                LHS += RHS;
            return LHS;
        }

        std::string VisitUnaryOperator(const UnaryOperator* E) {
            return printUnary(E->getOpcodeStr(E->getOpcode()), E->getSubExpr(), !E->isPostfix());
        }

        std::string VisitBinaryOperator(const BinaryOperator* E) {
            return printBinary(E->getOpcodeStr(E->getOpcode()), E->getLHS(), E->getRHS());
        }

        std::string VisitCXXOperatorCallExpr(const CXXOperatorCallExpr* E) {
            const char* Spelling = getOperatorSpelling(E->getOperator());
            // Handle weird unary-that-look-like-binary postfix operators.
            if((E->getOperator() == OO_PlusPlus || E->getOperator() == OO_MinusMinus) &&
               E->getNumArgs() == 2)
                return printUnary(Spelling, E->getArg(0), false);
            if(E->isInfixBinaryOp())
                return printBinary(Spelling, E->getArg(0), E->getArg(1));
            if(E->getNumArgs() == 1) {
                switch(E->getOperator()) {
                    case OO_Plus:
                    case OO_Minus:
                    case OO_Star:
                    case OO_Amp:
                    case OO_Tilde:
                    case OO_Exclaim:
                    case OO_PlusPlus:
                    case OO_MinusMinus: return printUnary(Spelling, E->getArg(0), true);
                    default: break;
                }
            }
            return "";
        }
    };

    return Namer{}.Visit(E);
}

// returns true for `X` in `template <typename... X> void foo()`
bool isTemplateTypeParameterPack(clang::NamedDecl* D) {
    if(const auto* TTPD = llvm::dyn_cast<clang::TemplateTypeParmDecl>(D)) {
        return TTPD->isParameterPack();
    }
    return false;
}

const clang::TemplateTypeParmType* getFunctionPackType(const clang::FunctionDecl* Callee) {
    if(const auto* TemplateDecl = Callee->getPrimaryTemplate()) {
        auto TemplateParams = TemplateDecl->getTemplateParameters()->asArray();
        // find the template parameter pack from the back
        const auto It = std::find_if(TemplateParams.rbegin(),
                                     TemplateParams.rend(),
                                     isTemplateTypeParameterPack);
        if(It != TemplateParams.rend()) {
            const auto* TTPD = llvm::dyn_cast<clang::TemplateTypeParmDecl>(*It);
            return TTPD->getTypeForDecl()->castAs<clang::TemplateTypeParmType>();
        }
    }
    return nullptr;
}

const clang::TemplateTypeParmType* getUnderlyingPackType(const clang::ParmVarDecl* Param) {
    const auto* PlainType = Param->getType().getTypePtr();
    if(auto* RT = llvm::dyn_cast<clang::ReferenceType>(PlainType))
        PlainType = RT->getPointeeTypeAsWritten().getTypePtr();
    if(const auto* SubstType = llvm::dyn_cast<clang::SubstTemplateTypeParmType>(PlainType)) {
        const auto* ReplacedParameter = SubstType->getReplacedParameter();
        if(ReplacedParameter->isParameterPack()) {
            return ReplacedParameter->getTypeForDecl()->castAs<clang::TemplateTypeParmType>();
        }
    }
    return nullptr;
}

// This visitor walks over the body of an instantiated function template.
// The template accepts a parameter pack and the visitor records whether
// the pack parameters were forwarded to another call. For example, given:
//
// template <typename T, typename... Args>
// auto make_unique(Args... args) {
//   return unique_ptr<T>(new T(args...));
// }
//
// When called as `make_unique<std::string>(2, 'x')` this yields a function
// `make_unique<std::string, int, char>` with two parameters.
// The visitor records that those two parameters are forwarded to the
// `constructor std::string(int, char);`.
//
// This information is recorded in the `ForwardingInfo` split into fully
// resolved parameters (passed as argument to a parameter that is not an
// expanded template type parameter pack) and forwarding parameters (passed to a
// parameter that is an expanded template type parameter pack).
class ForwardingCallVisitor : public clang::RecursiveASTVisitor<ForwardingCallVisitor> {
public:
    ForwardingCallVisitor(llvm::ArrayRef<const clang::ParmVarDecl*> Parameters) :
        Parameters{Parameters}, PackType{ast::getUnderlyingPackType(Parameters.front())} {}

    bool VisitCallExpr(clang::CallExpr* E) {
        auto* Callee = getCalleeDeclOrUniqueOverload(E);
        if(Callee) {
            handleCall(Callee, E->arguments());
        }
        return !Info.has_value();
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr* E) {
        auto* Callee = E->getConstructor();
        if(Callee) {
            handleCall(Callee, E->arguments());
        }
        return !Info.has_value();
    }

    // The expanded parameter pack to be resolved
    llvm::ArrayRef<const clang::ParmVarDecl*> Parameters;
    // The type of the parameter pack
    const clang::TemplateTypeParmType* PackType;

    struct ForwardingInfo {
        // If the parameters were resolved to another FunctionDecl, these are its
        // first non-variadic parameters (i.e. the first entries of the parameter
        // pack that are passed as arguments bound to a non-pack parameter.)
        llvm::ArrayRef<const clang::ParmVarDecl*> Head;
        // If the parameters were resolved to another FunctionDecl, these are its
        // variadic parameters (i.e. the entries of the parameter pack that are
        // passed as arguments bound to a pack parameter.)
        llvm::ArrayRef<const clang::ParmVarDecl*> Pack;
        // If the parameters were resolved to another FunctionDecl, these are its
        // last non-variadic parameters (i.e. the last entries of the parameter pack
        // that are passed as arguments bound to a non-pack parameter.)
        llvm::ArrayRef<const clang::ParmVarDecl*> Tail;
        // If the parameters were resolved to another FunctionDecl, this
        // is it.
        std::optional<clang::FunctionDecl*> PackTarget;
    };

    // The output of this visitor
    std::optional<ForwardingInfo> Info;

private:
    // inspects the given callee with the given args to check whether it
    // contains Parameters, and sets Info accordingly.
    void handleCall(clang::FunctionDecl* Callee, typename clang::CallExpr::arg_range Args) {
        // Skip functions with less parameters, they can't be the target.
        if(Callee->parameters().size() < Parameters.size())
            return;
        if(llvm::any_of(Args,
                        [](const clang::Expr* E) { return isa<clang::PackExpansionExpr>(E); })) {
            return;
        }
        auto PackLocation = findPack(Args);
        if(!PackLocation)
            return;
        llvm::ArrayRef<clang::ParmVarDecl*> MatchingParams =
            Callee->parameters().slice(*PackLocation, Parameters.size());
        // Check whether the function has a parameter pack as the last template
        // parameter
        if(const auto* TTPT = ast::getFunctionPackType(Callee)) {
            // In this case: Separate the parameters into head, pack and tail
            auto IsExpandedPack = [&](const clang::ParmVarDecl* P) {
                return ast::getUnderlyingPackType(P) == TTPT;
            };
            ForwardingInfo FI;
            FI.Head = MatchingParams.take_until(IsExpandedPack);
            FI.Pack = MatchingParams.drop_front(FI.Head.size()).take_while(IsExpandedPack);
            FI.Tail = MatchingParams.drop_front(FI.Head.size() + FI.Pack.size());
            FI.PackTarget = Callee;
            Info = FI;
            return;
        }
        // Default case: assume all parameters were fully resolved
        ForwardingInfo FI;
        FI.Head = MatchingParams;
        Info = FI;
    }

    // Returns the beginning of the expanded pack represented by Parameters
    // in the given arguments, if it is there.
    std::optional<size_t> findPack(typename clang::CallExpr::arg_range Args) {
        // find the argument directly referring to the first parameter
        assert(Parameters.size() <= static_cast<size_t>(llvm::size(Args)));
        for(auto Begin = Args.begin(), End = Args.end() - Parameters.size() + 1; Begin != End;
            ++Begin) {
            if(const auto* RefArg = unwrapForward(*Begin)) {
                if(Parameters.front() != RefArg->getDecl())
                    continue;
                // Check that this expands all the way until the last parameter.
                // It's enough to look at the last parameter, because it isn't possible
                // to expand without expanding all of them.
                auto ParamEnd = Begin + Parameters.size() - 1;
                RefArg = unwrapForward(*ParamEnd);
                if(!RefArg || Parameters.back() != RefArg->getDecl())
                    continue;
                return std::distance(Args.begin(), Begin);
            }
        }
        return std::nullopt;
    }

    static clang::FunctionDecl* getCalleeDeclOrUniqueOverload(clang::CallExpr* E) {
        clang::Decl* CalleeDecl = E->getCalleeDecl();
        auto* Callee = llvm::dyn_cast_or_null<clang::FunctionDecl>(CalleeDecl);
        if(!Callee) {
            if(auto* Lookup = dyn_cast<clang::UnresolvedLookupExpr>(E->getCallee())) {
                Callee = resolveOverload(Lookup, E);
            }
        }
        // Ignore the callee if the number of arguments is wrong (deal with va_args)
        if(Callee && Callee->getNumParams() == E->getNumArgs())
            return Callee;
        return nullptr;
    }

    static clang::FunctionDecl* resolveOverload(clang::UnresolvedLookupExpr* Lookup,
                                                clang::CallExpr* E) {
        clang::FunctionDecl* MatchingDecl = nullptr;
        if(!Lookup->requiresADL()) {
            // Check whether there is a single overload with this number of
            // parameters
            for(auto* Candidate: Lookup->decls()) {
                if(auto* FuncCandidate = llvm::dyn_cast_or_null<clang::FunctionDecl>(Candidate)) {
                    if(FuncCandidate->getNumParams() == E->getNumArgs()) {
                        if(MatchingDecl) {
                            // there are multiple candidates - abort
                            return nullptr;
                        }
                        MatchingDecl = FuncCandidate;
                    }
                }
            }
        }
        return MatchingDecl;
    }

    // Tries to get to the underlying argument by unwrapping implicit nodes and
    // std::forward.
    const static clang::DeclRefExpr* unwrapForward(const clang::Expr* E) {
        E = E->IgnoreImplicitAsWritten();
        // There might be an implicit copy/move constructor call on top of the
        // forwarded arg.
        // FIXME: Maybe mark implicit calls in the AST to properly filter here.
        if(const auto* Const = llvm::dyn_cast<clang::CXXConstructExpr>(E))
            if(Const->getConstructor()->isCopyOrMoveConstructor())
                E = Const->getArg(0)->IgnoreImplicitAsWritten();
        if(const auto* Call = llvm::dyn_cast<clang::CallExpr>(E)) {
            const auto Callee = Call->getBuiltinCallee();
            if(Callee == clang::Builtin::BIforward) {
                return llvm::dyn_cast<clang::DeclRefExpr>(
                    Call->getArg(0)->IgnoreImplicitAsWritten());
            }
        }
        return llvm::dyn_cast<clang::DeclRefExpr>(E);
    }
};

llvm::SmallVector<const clang::ParmVarDecl*>
    resolveForwardingParameters(const clang::FunctionDecl* D, unsigned MaxDepth) {
    auto Parameters = D->parameters();
    // If the function has a template parameter pack
    if(const auto* TTPT = ast::getFunctionPackType(D)) {
        // Split the parameters into head, pack and tail
        auto IsExpandedPack = [TTPT](const clang::ParmVarDecl* P) {
            return ast::getUnderlyingPackType(P) == TTPT;
        };
        llvm::ArrayRef<const clang::ParmVarDecl*> Head = Parameters.take_until(IsExpandedPack);
        llvm::ArrayRef<const clang::ParmVarDecl*> Pack =
            Parameters.drop_front(Head.size()).take_while(IsExpandedPack);
        llvm::ArrayRef<const clang::ParmVarDecl*> Tail =
            Parameters.drop_front(Head.size() + Pack.size());
        llvm::SmallVector<const clang::ParmVarDecl*> Result(Parameters.size());
        // Fill in non-pack parameters
        auto* HeadIt = std::copy(Head.begin(), Head.end(), Result.begin());
        auto TailIt = std::copy(Tail.rbegin(), Tail.rend(), Result.rbegin());
        // Recurse on pack parameters

        size_t Depth = 0;

        const clang::FunctionDecl* CurrentFunction = D;
        llvm::SmallSet<const clang::FunctionTemplateDecl*, 4> SeenTemplates;
        if(const auto* Template = D->getPrimaryTemplate()) {
            SeenTemplates.insert(Template);
        }

        while(!Pack.empty() && CurrentFunction && Depth < MaxDepth) {
            // Find call expressions involving the pack
            ForwardingCallVisitor V{Pack};
            V.TraverseStmt(CurrentFunction->getBody());
            if(!V.Info) {
                break;
            }
            // If we found something: Fill in non-pack parameters
            auto Info = *V.Info;
            HeadIt = std::copy(Info.Head.begin(), Info.Head.end(), HeadIt);
            TailIt = std::copy(Info.Tail.rbegin(), Info.Tail.rend(), TailIt);
            // Prepare next recursion level
            Pack = Info.Pack;
            CurrentFunction = Info.PackTarget.value_or(nullptr);
            Depth++;
            // If we are recursing into a previously encountered function: Abort
            if(CurrentFunction) {
                if(const auto* Template = CurrentFunction->getPrimaryTemplate()) {
                    bool NewFunction = SeenTemplates.insert(Template).second;
                    if(!NewFunction) {
                        return {Parameters.begin(), Parameters.end()};
                    }
                }
            }
        }

        // Fill in the remaining unresolved pack parameters
        HeadIt = std::copy(Pack.begin(), Pack.end(), HeadIt);
        assert(TailIt.base() == HeadIt);
        return Result;
    }
    return {Parameters.begin(), Parameters.end()};
}

bool isSugaredTemplateParameter(clang::QualType QT) {
    static auto PeelWrapper = [](clang::QualType QT) {
        // Neither `PointerType` nor `ReferenceType` is considered as sugared
        // type. Peel it.
        clang::QualType Peeled = QT->getPointeeType();
        return Peeled.isNull() ? QT : Peeled;
    };

    // This is a bit tricky: we traverse the type structure and find whether or
    // not a type in the desugaring process is of SubstTemplateTypeParmType.
    // During the process, we may encounter pointer or reference types that are
    // not marked as sugared; therefore, the desugar function won't apply. To
    // move forward the traversal, we retrieve the pointees using
    // QualType::getPointeeType().
    //
    // However, getPointeeType could leap over our interests: The QT::getAs<T>()
    // invoked would implicitly desugar the type. Consequently, if the
    // SubstTemplateTypeParmType is encompassed within a TypedefType, we may lose
    // the chance to visit it.
    // For example, given a QT that represents `std::vector<int *>::value_type`:
    //  `-ElaboratedType 'value_type' sugar
    //    `-TypedefType 'vector<int *>::value_type' sugar
    //      |-Typedef 'value_type'
    //      `-SubstTemplateTypeParmType 'int *' sugar class depth 0 index 0 T
    //        |-ClassTemplateSpecialization 'vector'
    //        `-PointerType 'int *'
    //          `-BuiltinType 'int'
    // Applying `getPointeeType` to QT results in 'int', a child of our target
    // node SubstTemplateTypeParmType.
    //
    // As such, we always prefer the desugared over the pointee for next type
    // in the iteration. It could avoid the getPointeeType's implicit desugaring.
    while(true) {
        if(QT->getAs<clang::SubstTemplateTypeParmType>())
            return true;
        clang::QualType Desugared = QT->getLocallyUnqualifiedSingleStepDesugaredType();
        if(Desugared != QT)
            QT = Desugared;
        else if(auto Peeled = PeelWrapper(Desugared); Peeled != QT)
            QT = Peeled;
        else
            break;
    }

    return false;
}

std::optional<clang::QualType> desugar(clang::ASTContext& AST, clang::QualType QT) {
    bool ShouldAKA = false;
    auto Desugared = clang::desugarForDiagnostic(AST, QT, ShouldAKA);
    if(!ShouldAKA)
        return std::nullopt;
    return Desugared;
}

clang::QualType maybeDesugar(clang::ASTContext& AST, clang::QualType QT) {
    // Prefer desugared type for name that aliases the template parameters.
    // This can prevent things like printing opaque `: type` when accessing std
    // containers.
    if(ast::isSugaredTemplateParameter(QT))
        return desugar(AST, QT).value_or(QT);

    // Prefer desugared type for `decltype(expr)` specifiers.
    if(QT->isDecltypeType())
        return QT.getCanonicalType();

    if(const auto* AT = QT->getContainedAutoType())
        if(!AT->getDeducedType().isNull() && AT->getDeducedType()->isDecltypeType())
            return QT.getCanonicalType();

    return QT;
}

clang::FunctionProtoTypeLoc getPrototypeLoc(clang::Expr* Fn) {
    clang::TypeLoc Target;
    clang::Expr* NakedFn = Fn->IgnoreParenCasts();
    if(const auto* T = NakedFn->getType().getTypePtr()->getAs<clang::TypedefType>()) {
        Target = T->getDecl()->getTypeSourceInfo()->getTypeLoc();
    } else if(const auto* DR = llvm::dyn_cast<clang::DeclRefExpr>(NakedFn)) {
        const auto* D = DR->getDecl();
        if(const auto* const VD = llvm::dyn_cast<clang::VarDecl>(D)) {
            Target = VD->getTypeSourceInfo()->getTypeLoc();
        }
    }

    if(!Target)
        return {};

    // Unwrap types that may be wrapping the function type
    while(true) {
        if(auto P = Target.getAs<clang::PointerTypeLoc>()) {
            Target = P.getPointeeLoc();
            continue;
        }
        if(auto A = Target.getAs<clang::AttributedTypeLoc>()) {
            Target = A.getModifiedLoc();
            continue;
        }
        if(auto P = Target.getAs<clang::ParenTypeLoc>()) {
            Target = P.getInnerLoc();
            continue;
        }
        break;
    }

    if(auto F = Target.getAs<clang::FunctionProtoTypeLoc>()) {
        return F;
    }

    return {};
}

}  // namespace clice::ast
