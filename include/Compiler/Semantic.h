#include "Compiler.h"
#include "Resolver.h"

#include "Support/Enum.h"
#include "Support/FileSystem.h"
#include "Utility.h"

namespace clice {

enum class RelationKinds : uint32_t {
    Invalid,
    Declaration,
    Definition,
    Reference,
    // Write Relation.
    Read,
    Write,
    Interface,
    Implementation,
    /// When target is a type definition of source, source is possible type or constructor.
    TypeDefinition,

    /// When target is a base class of source.
    Base,
    /// When target is a derived class of source.
    Derived,

    /// When target is a constructor of source.
    Constructor,
    /// When target is a destructor of source.
    Destructor,

    // When target is a caller of source.
    Caller,
    // When target is a callee of source.
    Callee,
};

struct RelationKind : enum_type<RelationKinds, true> {
    using enum RelationKinds;
    using enum_type::enum_type;
    using enum_type::operator=;
};

enum class OccurrenceKind {
    /// This occurrence directly corresponds to a unique source symbol.
    Source,
    /// This occurrence is a macro expansion.
    MacroExpansion,
    /// This occurrence is from `PseudoInstantiation` and may be not correct.
    PseudoInstantiation,
    /// This occurrence is from `ImplicitInstantiation` or `ExplicitInstantiation` of a template.
    Instantiation,
};

class SemanticVisitor : public clang::RecursiveASTVisitor<SemanticVisitor> {
public:
    using Base = clang::RecursiveASTVisitor<SemanticVisitor>;

    SemanticVisitor(Compiler& compiler) :
        sema(compiler.sema()), resolver(compiler.resolver()), srcMgr(compiler.srcMgr()) {}

public:
    /// ============================================================================
    ///                                Declaration
    /// ============================================================================

    TRAVERSE_DECL(Decl) {
        decls.push_back(decl);
        auto result = Base::TraverseDecl(decl);
        decls.pop_back();
        return result;
    }

    VISIT_DECL(NamespaceDecl) {
        /// `namespace Foo { }`
        ///             ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(NamespaceAliasDecl) {
        /// `namespace Foo = Bar`
        ///             ^     ^~~~ reference
        ///             ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        handleOccurrence(decl->getNamespace(), decl->getTargetNameLoc());
        return true;
    }

    VISIT_DECL(UsingDirectiveDecl) {
        /// `using namespace Foo`
        ///                   ^~~~~~~ reference
        handleOccurrence(decl->getNominatedNamespace(), decl->getLocation());
        return true;
    }

    VISIT_DECL(LabelDecl) {
        /// `label:`
        ///    ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(FieldDecl) {
        /// `int foo;`
        ///       ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(EnumConstantDecl) {
        /// `enum Foo { bar };`
        ///              ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(UsingDecl) {
        /// `using Foo::bar;`
        ///              ^~~~ reference
        handleOccurrence(decl, decl->getLocation());
        /// FIXME:
        return true;
    }

    VISIT_DECL(BindingDecl) {
        /// `auto [a, b] = std::make_tuple(1, 2);`
        ///        ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(TemplateTypeParmDecl) {
        /// `template <typename T>`
        ///                     ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(TemplateTemplateParmDecl) {
        /// `template <template <typename> class T>`
        ///                                      ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(NonTypeTemplateParmDecl) {
        /// `template <int N>`
        ///                ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(TagDecl) {
        /// `Base::TraverseClassTemplate...Decl` will also traverse it's templated class.
        /// We already handled the template class in `VisitClassTemplate...Decl`. So we skip
        /// them here.
        if(decl->getDescribedTemplate() ||
           llvm::isa<clang::ClassTemplateSpecializationDecl,
                     clang::ClassTemplatePartialSpecializationDecl>(decl)) {
            return true;
        }

        /// `struct/class/union/enum Foo { };`
        ///                           ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(ClassTemplateDecl) {
        /// `template <typename T> class Foo { };`
        ///                               ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(ClassTemplatePartialSpecializationDecl) {
        /// It's possible that a class template specialization is a full specialization or a
        /// explicit instantiation. And `ClassTemplatePartialSpecializationDecl` is the subclass of
        /// `ClassTemplateSpecializationDecl`, it is also handled here.
        ///
        /// For full specialization:
        /// `template <> class Foo<int> { };`
        ///                     ^~~~ declaration/definition
        ///
        /// For explicit instantiation:
        /// `template class Foo<int>;`
        ///                  ^~~~ reference
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(FunctionDecl) {
        /// Because `TraverseFunctionTemplateDecl` will also traverse it's templated function. We
        /// already handled the template function in `VisitFunctionTemplateDecl`. So we skip them
        /// here.
        if(decl->getDescribedFunctionTemplate()) {
            return true;
        }

        /// `void foo();`
        ///         ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(FunctionTemplateDecl) {
        /// `template <typename T> void foo();`
        ///                               ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());

        /// `template void foo<int>();`
        ///                  ^~~~ reference

        /// FIXME: Clang currently doesn't record the information of explicit instantiation
        /// correctly. See https://github.com/llvm/llvm-project/issues/115418. And it is not added
        /// to its lexical context. So here we use a workaround to handle explicit instantiation of
        /// function template.

        // for(auto spec: decl->specializations()) {
        //     auto kind = spec->getTemplateSpecializationKind();
        //     if(kind == clang::TSK_ExplicitInstantiationDeclaration ||
        //        kind == clang::TSK_ExplicitInstantiationDefinition) {
        //         /// WORKAROUND: Clang currently doesn't record the location of explicit
        //         /// instantiation. Use the location of the point of instantiation instead.
        //         if(auto location = builder.addLocation(spec->getPointOfInstantiation())) {
        //             auto symbol = builder.addSymbol(decl);
        //             symbol.addOccurrence(location);
        //             symbol.addReference(location);
        //         }
        //     }
        // }

        return true;
    }

    VISIT_DECL(TypedefNameDecl) {
        /// Skip the typedef name declaration that is a type alias template.
        if(decl->getDescribedTemplate()) {
            return true;
        }

        /// `using Foo = int;`
        ///             ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(TypeAliasTemplateDecl) {
        /// FIXME: the location of type alias template is not recorded correctly, actually
        /// it is the location of using keyword. But location its templated type is correct.
        /// Temporarily use the location of the templated type.

        /// `template <typename T> using Foo = T;`
        ///                               ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(VarDecl) {
        /// Because `TraverseVarTemplateSpecializationDecl` will also traverse it's templated
        /// variable. We already handled the template variable in `VisitVar...`.
        /// So we skip them here.
        if(decl->getDescribedVarTemplate() ||
           llvm::isa<clang::VarTemplateSpecializationDecl,
                     clang::VarTemplatePartialSpecializationDecl>(decl)) {
            return true;
        }

        /// `int foo;`
        ///       ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(VarTemplateDecl) {
        /// `template <typename T> int foo;`
        ///                             ^~~~ declaration/definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(VarTemplateSpecializationDecl) {
        /// FIXME: it's strange that `VarTemplateSpecializationDecl` occurs in the lexical context.
        /// This should be a bug of clang. Skip it here. And clang also doesn't record the
        /// information about explicit instantiation of var template correctly. Skip them
        /// temporarily.
        auto kind = decl->getSpecializationKind();
        if(kind == clang::TSK_ImplicitInstantiation ||
           kind == clang::TSK_ExplicitInstantiationDeclaration ||
           kind == clang::TSK_ExplicitInstantiationDefinition) {
            return true;
        }

        /// It's possible that a var template specialization is a full specialization or a explicit
        /// instantiation. And `VarTemplatePartialSpecializationDecl` is the subclass of
        /// `VarTemplateSpecializationDecl`, it is also handled here.
        ///
        /// For full specialization:
        /// `template <> int foo<int>;`
        ///                    ^~~~ declaration/definition
        ///
        /// For explicit instantiation:
        /// `template int foo<int>;`
        ///                ^~~~ reference
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(ConceptDecl) {
        /// `template <typename T> concept Foo = ...;`
        ///                                 ^~~~ definition
        handleOccurrence(decl, decl->getLocation());
        return true;
    }

    bool TraverseConceptReference(clang::ConceptReference* reference) {
        /// `requires Foo<T>;`
        ///            ^~~~ reference
        handleOccurrence(reference->getNamedConcept(), reference->getConceptNameLoc());

        return Base::TraverseConceptReference(reference);
    }

    /// ============================================================================
    ///                                  TypeLoc
    /// ============================================================================

    /// We don't care about type without location information.
    constexpr bool TraverseType [[gnu::const]] (clang::QualType) {
        return true;
    }

    VISIT_TYPELOC(TagTypeLoc) {
        /// `struct Foo { }; Foo foo`
        ///                   ^~~~ reference
        handleOccurrence(loc.getDecl(), loc.getNameLoc());
        return true;
    }

    VISIT_TYPELOC(TypedefTypeLoc) {
        /// `using Foo = int; Foo foo`
        ///                    ^~~~ reference
        handleOccurrence(loc.getTypedefNameDecl(), loc.getNameLoc());
        return true;
    }

    VISIT_TYPELOC(TemplateTypeParmTypeLoc) {
        /// `template <typename T> void foo(T t)`
        ///                                 ^~~~ reference
        handleOccurrence(loc.getDecl(), loc.getNameLoc());
        return true;
    }

    VISIT_TYPELOC(TemplateSpecializationTypeLoc) {
        /// `std::vector<int>`
        ///        ^~~~ reference
        handleOccurrence(declForType(loc.getType()), loc.getTemplateNameLoc());
        return true;
    }

    VISIT_TYPELOC(DependentNameTypeLoc) {
        /// `std::vector<T>::value_type`
        ///                      ^~~~ reference
        for(auto decl: resolver.lookup(loc.getTypePtr())) {
            handleOccurrence(decl, loc.getNameLoc());
        }
        return true;
    }

    VISIT_TYPELOC(DependentTemplateSpecializationTypeLoc) {
        /// `std::allocator<T>::rebind<U>`
        ///                       ^~~~ reference
        for(auto decl: resolver.lookup(loc.getTypePtr())) {
            handleOccurrence(decl, loc.getTemplateNameLoc());
        }
        return true;
    }

    /// ============================================================================
    ///                                Specifier
    /// ============================================================================

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return Base::TraverseCXXBaseSpecifier(base);
    }

    /// We don't care about name specifier without location information.
    constexpr bool TraverseNestedNameSpecifier [[gnu::const]] (clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
        if(!loc) {
            return true;
        }

        auto NNS = loc.getNestedNameSpecifier();
        switch(NNS->getKind()) {
            case clang::NestedNameSpecifier::Namespace: {
                handleOccurrence(NNS->getAsNamespace(), loc.getLocalBeginLoc());
                break;
            }
            case clang::NestedNameSpecifier::NamespaceAlias: {
                handleOccurrence(NNS->getAsNamespaceAlias(), loc.getLocalBeginLoc());
                break;
            }
            case clang::NestedNameSpecifier::Identifier: {
                assert(NNS->isDependent() && "Identifier NNS should be dependent");
                // FIXME: use TemplateResolver here.
                break;
            }
            case clang::NestedNameSpecifier::TypeSpec:
            case clang::NestedNameSpecifier::TypeSpecWithTemplate:
            case clang::NestedNameSpecifier::Global:
            case clang::NestedNameSpecifier::Super: {
                break;
            };
        }

        return Base::TraverseNestedNameSpecifierLoc(loc);
    }

    bool TraverseConstructorInitializer(clang::CXXCtorInitializer* init) {
        return Base::TraverseConstructorInitializer(init);
    }

    /// ============================================================================
    ///                                 Statement
    /// ============================================================================

    VISIT_EXPR(DeclRefExpr) {
        /// `foo = 1`
        ///   ^~~~ reference
        handleOccurrence(expr->getDecl(), expr->getLocation());
        return true;
    }

    VISIT_EXPR(MemberExpr) {
        /// `foo.bar`
        ///       ^~~~ reference
        handleOccurrence(expr->getMemberDecl(), expr->getMemberLoc());
        return true;
    }

    VISIT_EXPR(DependentScopeDeclRefExpr) {
        /// `std::is_same<T, U>::value`
        ///                        ^~~~ reference
        for(auto decl: resolver.lookup(expr)) {
            handleOccurrence(decl, expr->getNameInfo().getLoc());
        }
        return true;
    }

    VISIT_EXPR(CXXDependentScopeMemberExpr) {
        /// `std::is_same<T, U>::value.T::value`
        ///                                 ^~~~ reference

        return true;
    }

    VISIT_EXPR(CallExpr) {
        // TODO: consider lambda expression.
        return true;
    }

public:
    consteval bool VisitImplicitInstantiation() {
        return true;
    }

    /// An occurrence directly corresponding to a symbol in source code.
    /// In most cases, a location just correspondings to unique decl.
    /// So a location will be just visited once. But in some other cases,
    /// a location may correspond to multiple decls. Note that we already
    /// filter some nodes with invalid location.
    ///
    /// Always uses spelling location if the original location is a macro location.
    void handleOccurrence(const clang::Decl* decl,
                          clang::SourceLocation location,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        if(location.isMacroID()) {
            if(srcMgr.isMacroArgExpansion(location)) {
                auto spelling = srcMgr.getSpellingLoc(location);
                dump(spelling);
                auto expansion = srcMgr.getExpansionLoc(location);
                dump(expansion);
                llvm::outs() << "-----------------------------------------------------\n";
            }
        }
    }

    void dump(clang::SourceLocation loc) {
        auto location = srcMgr.getPresumedLoc(loc);
        llvm::SmallString<128> path;
        auto err = fs::real_path(location.getFilename(), path);
        llvm::outs() << path << ":" << location.getLine() << ":" << location.getColumn() << "\n";
    }

    /// Always uses expansion location if the original location is a macro location.
    void handleRelation(const clang::Decl* decl, RelationKind kind, clang::SourceRange range) {
        ///
    }

private:
    clang::Sema& sema;
    TemplateResolver& resolver;
    clang::SourceManager& srcMgr;
    llvm::SmallVector<clang::Decl*> decls;
};

}  // namespace clice
