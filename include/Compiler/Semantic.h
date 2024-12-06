#pragma once

#include "Compiler.h"
#include "Resolver.h"
#include "Utility.h"

#include "Support/Support.h"

namespace clice {

/// In the LSP, there are several different kinds, such as `SemanticTokenType`,
/// `CompletionItemKind`, and `SymbolKind`. Unfortunately, these kinds do not cover all the semantic
/// information we need. It's also inconsistent that some kinds exist in one category but not in
/// another, for example, `Namespace` is present in `SemanticTokenType` but not in
/// `CompletionItemKind`. To address this, we define our own `SymbolKind`, which will be used
/// consistently across our responses to the client and in the index. Users who prefer to stick to
/// standard LSP kinds can map our `SymbolKind` to the corresponding LSP kinds through
/// configuration.
struct SymbolKind : support::Enum<SymbolKind, false, uint8_t> {
    enum Kind : uint8_t {
        Comment = 0,     ///< C/C++ comments.
        Number,          ///< C/C++ number literal.
        Character,       ///< C/C++ character literal.
        String,          ///< C/C++ string literal.
        Keyword,         ///< C/C++ keyword.
        Directive,       ///< C/C++ preprocessor directive, e.g. `#include`.
        Header,          ///< C/C++ header name, e.g. `<iostream>` and `"foo.h"`.
        Module,          ///< C++20 module name.
        Macro,           ///< C/C++ macro.
        MacroParameter,  ///< C/C++ macro parameter.
        Namespace,       ///> C++ namespace.
        Class,           ///> C/C++ class.
        Struct,          ///> C/C++ struct.
        Union,           ///> C/C++ union.
        Enum,            ///> C/C++ enum.
        Type,            ///> C/C++ type alias and C++ template type parameter.
        Field,           ///> C/C++ field.
        EnumMember,      ///> C/C++ enum member.
        Function,        ///> C/C++ function.
        Method,          ///> C++ method.
        Variable,        ///> C/C++ variable, includes C++17 structured bindings.
        Parameter,       ///> C/C++ parameter.
        Label,           ///> C/C++ label.
        Concept,         ///> C++20 concept.
        Attribute,       ///> GNU/MSVC/C++11/C23 attribute.
        Operator,        ///> C/C++ operator.
        Paren,           ///> `(` and `)`.
        Bracket,         ///> `[` and `]`.
        Brace,           ///> `{` and `}`.
        Angle,           ///> `<` and `>`.
        Invalid,
    };

    using Enum::Enum;

    constexpr inline static auto InvalidEnum = Kind::Invalid;

    static SymbolKind from(const clang::Decl* decl);

    static SymbolKind from(const clang::tok::TokenKind kind);
};

/// A bit field enum to describe the kind of relation between two symbols.
struct RelationKind : support::Enum<RelationKind, true, uint32_t> {
    enum Kind : uint32_t {
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

    using Enum::Enum;
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

template <typename Derived>
class SemanticVisitor : public clang::RecursiveASTVisitor<SemanticVisitor<Derived>> {
public:
    using Base = clang::RecursiveASTVisitor<SemanticVisitor>;

    SemanticVisitor(ASTInfo& info, bool mainFileOnly = false) :
        sema(info.sema()), pp(info.pp()), resolver(info.resolver()), srcMgr(info.srcMgr()),
        tokBuf(info.tokBuf()), mainFileOnly(mainFileOnly) {}

public:

public:
    consteval bool VisitImplicitInstantiation() {
        return true;
    }

    Derived& getDerived() {
        return static_cast<Derived&>(*this);
    }

    bool needFilter(clang::SourceLocation location) {
        return location.isInvalid() || (mainFileOnly && !srcMgr.isInMainFile(location));
    }

    void dump [[gnu::noinline]] (clang::SourceLocation loc) {
        auto location = srcMgr.getPresumedLoc(loc);
        llvm::SmallString<128> path;
        auto err = fs::real_path(location.getFilename(), path);
        llvm::outs() << path << ":" << location.getLine() << ":" << location.getColumn() << "\n";
    }

    /// An occurrence directly corresponding to a symbol in source code.
    /// In most cases, a location just correspondings to unique decl.
    /// So a location will be just visited once. But in some other cases,
    /// a location may correspond to multiple decls. Note that we already
    /// filter some nodes with invalid location.
    ///
    /// Always uses spelling location if the original location is a macro location.
    void handleOccurrence(const clang::Decl* decl, clang::SourceRange range, RelationKind kind) {}

    /// Builtin type doesn't have corresponding decl. So we handle it separately.
    /// And it is possible that a builtin type is composed of multiple tokens.
    /// e.g. `unsigned long long`.
    void handleOccurrence(const clang::BuiltinType* type,
                          clang::SourceRange range,
                          OccurrenceKind kind = OccurrenceKind::Source) {}

    void handleOccurrence(const clang::Attr* attr, clang::SourceRange range) {}

public:
    /// ============================================================================
    ///                                Declaration
    /// ============================================================================

    TRAVERSE_DECL(Decl) {
        if(!decl) {
            return true;
        }

        if(!llvm::isa<clang::TranslationUnitDecl>(decl) && needFilter(decl->getLocation())) {
            return true;
        }

        decls.push_back(decl);
        auto result = Base::TraverseDecl(decl);
        decls.pop_back();
        return result;
    }

    VISIT_DECL(NamespaceDecl) {
        /// `namespace Foo { }`
        ///             ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    VISIT_DECL(NamespaceAliasDecl) {
        /// `namespace Foo = Bar`
        ///             ^     ^~~~ reference
        ///             ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        getDerived().handleOccurrence(decl->getNamespace(),
                                      decl->getTargetNameLoc(),
                                      RelationKind::Reference);
        return true;
    }

    VISIT_DECL(UsingDirectiveDecl) {
        /// `using namespace Foo`
        ///                   ^~~~~~~ reference
        getDerived().handleOccurrence(decl->getNominatedNamespace(),
                                      decl->getLocation(),
                                      RelationKind::Reference);
        return true;
    }

    VISIT_DECL(LabelDecl) {
        /// `label:`
        ///    ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    VISIT_DECL(FieldDecl) {
        /// `int foo;`
        ///       ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        /// FIXME: add Type Definition
        return true;
    }

    VISIT_DECL(EnumConstantDecl) {
        /// `enum Foo { bar };`
        ///              ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        /// FIXME: add Type Definition
        return true;
    }

    VISIT_DECL(UsingDecl) {
        /// `using Foo::bar;`
        ///              ^~~~ reference
        for(auto shadow: decl->shadows()) {
            getDerived().handleOccurrence(shadow, decl->getLocation(), RelationKind::Reference);
        }
        return true;
    }

    VISIT_DECL(BindingDecl) {
        /// `auto [a, b] = std::make_tuple(1, 2);`
        ///        ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        /// FIXME: add Type Definition
        return true;
    }

    VISIT_DECL(TemplateTypeParmDecl) {
        /// `template <typename T>`
        ///                     ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    VISIT_DECL(TemplateTemplateParmDecl) {
        /// `template <template <typename> class T>`
        ///                                      ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    VISIT_DECL(NonTypeTemplateParmDecl) {
        /// `template <int N>`
        ///                ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    VISIT_DECL(TagDecl) {
        /// FIXME:
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
        // if(decl->getDescribedTemplate() ||
        //    llvm::isa<clang::ClassTemplateSpecializationDecl,
        //              clang::ClassTemplatePartialSpecializationDecl>(decl)) {
        //     return true;
        // }

        /// `struct/class/union/enum Foo { };`
        ///                           ^~~~ declaration/definition
        getDerived().handleOccurrence(decl,
                                      decl->getLocation(),
                                      decl->isThisDeclarationADefinition()
                                          ? RelationKind::Definition
                                          : RelationKind::Declaration);
        return true;
    }

    VISIT_DECL(FunctionDecl) {
        /// FIXME:
        /// Because `TraverseFunctionTemplateDecl` will also traverse it's templated function. We
        /// already handled the template function in `VisitFunctionTemplateDecl`. So we skip them
        /// here.
        // if(decl->getDescribedFunctionTemplate()) {
        //     return true;
        // }

        /// `void foo();`
        ///         ^~~~ declaration/definition
        getDerived().handleOccurrence(decl,
                                      decl->getLocation(),
                                      decl->isThisDeclarationADefinition()
                                          ? RelationKind::Definition
                                          : RelationKind::Declaration);
        return true;
    }

    VISIT_DECL(FunctionTemplateDecl) {
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
        /// FIXME: the location of type alias template is not recorded correctly, actually
        /// it is the location of using keyword. But location its templated type is correct.
        /// Temporarily use the location of the templated type.

        /// `using Foo = int;`
        ///             ^~~~ declaration/definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        /// FIXME: add type definition
        return true;
    }

    VISIT_DECL(VarDecl) {
        /// FIXME:
        /// Because `TraverseVarTemplateSpecializationDecl` will also traverse it's templated
        /// variable. We already handled the template variable in `VisitVar...`.
        /// So we skip them here.
        // if(decl->getDescribedVarTemplate() ||
        //    llvm::isa<clang::VarTemplateSpecializationDecl,
        //              clang::VarTemplatePartialSpecializationDecl>(decl)) {
        //     return true;
        // }

        /// `int foo;`
        ///       ^~~~ declaration/definition
        getDerived().handleOccurrence(decl,
                                      decl->getLocation(),
                                      decl->isThisDeclarationADefinition()
                                          ? RelationKind::Definition
                                          : RelationKind::Declaration);
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

        /// FIXME:
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
        /// getDerived().handleOccurrence(decl, decl->getLocation());
        return true;
    }

    VISIT_DECL(ConceptDecl) {
        /// `template <typename T> concept Foo = ...;`
        ///                                 ^~~~ definition
        getDerived().handleOccurrence(decl, decl->getLocation(), RelationKind::Definition);
        return true;
    }

    bool TraverseConceptReference(clang::ConceptReference* reference) {
        /// `requires Foo<T>;`
        ///            ^~~~ reference
        getDerived().handleOccurrence(reference->getNamedConcept(),
                                      reference->getConceptNameLoc(),
                                      RelationKind::Reference);

        return Base::TraverseConceptReference(reference);
    }

    /// ============================================================================
    ///                                  TypeLoc
    /// ============================================================================

    /// We don't care about type without location information.
    constexpr bool TraverseType [[gnu::const]] (clang::QualType) {
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        /// FIXME: Workaround for `QualifiedTypeLoc`.
        if(auto QL = loc.getAs<clang::QualifiedTypeLoc>()) {
            return Base::TraverseTypeLoc(QL.getUnqualifiedLoc());
        }

        if(needFilter(loc.getLocalSourceRange().getBegin())) {
            return true;
        }

        return Base::TraverseTypeLoc(loc);
    }

    VISIT_TYPELOC(BuiltinTypeLoc) {
        /// `int foo`
        ///    ^~~~ reference
        getDerived().handleOccurrence(loc.getTypePtr(), loc.getLocalSourceRange());
        return true;
    }

    VISIT_TYPELOC(TagTypeLoc) {
        /// `struct Foo { }; Foo foo`
        ///                   ^~~~ reference
        getDerived().handleOccurrence(loc.getDecl(), loc.getNameLoc(), RelationKind::Reference);
        return true;
    }

    VISIT_TYPELOC(TypedefTypeLoc) {
        /// `using Foo = int; Foo foo`
        ///                    ^~~~ reference
        getDerived().handleOccurrence(loc.getTypedefNameDecl(),
                                      loc.getNameLoc(),
                                      RelationKind::Reference);
        return true;
    }

    VISIT_TYPELOC(TemplateTypeParmTypeLoc) {
        /// `template <typename T> void foo(T t)`
        ///                                 ^~~~ reference
        getDerived().handleOccurrence(loc.getDecl(), loc.getNameLoc(), RelationKind::Reference);
        return true;
    }

    VISIT_TYPELOC(TemplateSpecializationTypeLoc) {
        /// `std::vector<int>`
        ///        ^~~~ reference
        getDerived().handleOccurrence(declForType(loc.getType()),
                                      loc.getTemplateNameLoc(),
                                      RelationKind::Reference);
        return true;
    }

    VISIT_TYPELOC(DependentNameTypeLoc) {
        /// `std::vector<T>::value_type`
        ///                      ^~~~ reference
        for(auto decl: resolver.lookup(loc.getTypePtr())) {
            getDerived().handleOccurrence(decl, loc.getNameLoc(), RelationKind::Reference);
        }
        return true;
    }

    VISIT_TYPELOC(DependentTemplateSpecializationTypeLoc) {
        /// `std::allocator<T>::rebind<U>`
        ///                       ^~~~ reference
        for(auto decl: resolver.lookup(loc.getTypePtr())) {
            getDerived().handleOccurrence(decl, loc.getTemplateNameLoc(), RelationKind::Reference);
        }
        return true;
    }

    /// ============================================================================
    ///                                Specifier
    /// ============================================================================

    bool TraverseAttr(clang::Attr* attr) {
        if(needFilter(attr->getLocation())) {
            return true;
        }

        getDerived().handleOccurrence(attr, attr->getLocation());

        return Base::TraverseAttr(attr);
    }

    /// FIXME: clang currently doesn't traverse attributes in `AttrbutedStmt` correctly.
    /// See https://github.com/llvm/llvm-project/issues/117687.
    bool TraverseAttributedStmt(clang::AttributedStmt* stmt) {
        if(needFilter(stmt->getBeginLoc())) {
            return true;
        }

        for(auto attr: stmt->getAttrs()) {
            getDerived().handleOccurrence(attr, attr->getRange());
        }

        return Base::TraverseAttributedStmt(stmt);
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return Base::TraverseCXXBaseSpecifier(base);
    }

    /// We don't care about name specifier without location information.
    constexpr bool TraverseNestedNameSpecifier [[gnu::const]] (clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
        if(!loc || needFilter(loc.getLocalBeginLoc())) {
            return true;
        }

        auto NNS = loc.getNestedNameSpecifier();
        switch(NNS->getKind()) {
            case clang::NestedNameSpecifier::Namespace: {
                getDerived().handleOccurrence(NNS->getAsNamespace(),
                                              loc.getLocalBeginLoc(),
                                              RelationKind::Reference);
                break;
            }
            case clang::NestedNameSpecifier::NamespaceAlias: {
                getDerived().handleOccurrence(NNS->getAsNamespaceAlias(),
                                              loc.getLocalBeginLoc(),
                                              RelationKind::Reference);
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

    bool TraverseStmt(clang::Stmt* stmt) {
        if(stmt && needFilter(stmt->getBeginLoc())) {
            return true;
        }

        return Base::TraverseStmt(stmt);
    }

    VISIT_EXPR(CallExpr) {
        // TODO: consider lambda expression.
        auto caller = decls.back();
        if(llvm::isa<clang::StaticAssertDecl>(caller)) {
            caller = llvm::cast<clang::NamedDecl>(caller->getDeclContext());
        }

        auto callee = expr->getCalleeDecl();
        if(callee && caller) {
            getDerived().handleOccurrence(caller, expr->getSourceRange(), RelationKind::Caller);
            getDerived().handleOccurrence(callee, expr->getSourceRange(), RelationKind::Callee);
        }
        return true;
    }

    VISIT_EXPR(DeclRefExpr) {
        /// `foo = 1`
        ///   ^~~~ reference
        getDerived().handleOccurrence(expr->getDecl(),
                                      expr->getLocation(),
                                      RelationKind::Reference);
        return true;
    }

    VISIT_EXPR(MemberExpr) {
        /// `foo.bar`
        ///       ^~~~ reference
        if(expr->getMemberLoc().isValid()) {
            /// FIXME: if the location of member loc is invalid, this represents it is a
            /// implicit member expr, e.g. `if(x)`, implicit `if(x.operator bool())`. Try to
            /// use parens around the member loc.
            getDerived().handleOccurrence(expr->getMemberDecl(),
                                          expr->getMemberLoc(),
                                          RelationKind::Reference);
        }
        return true;
    }

    VISIT_EXPR(UnresolvedLookupExpr) {
        /// `std::is_same<T, U>::value`
        ///           ^~~~ reference
        for(auto decl: resolver.lookup(expr)) {
            getDerived().handleOccurrence(decl, expr->getNameLoc(), RelationKind::Reference);
        }
        return true;
    }

    VISIT_EXPR(DependentScopeDeclRefExpr) {
        /// `std::is_same<T, U>::value`
        ///                        ^~~~ reference
        /// dump(expr->getLocation());
        /// FIXME:
        /// TemplateResolver::debug = true;
        // for(auto decl: resolver.lookup(expr)) {
        //     getDerived().handleOccurrence(decl,
        //                                   expr->getNameInfo().getLoc(),
        //                                   RelationKind::Reference);
        // }
        return true;
    }

    VISIT_EXPR(CXXDependentScopeMemberExpr) {
        /// `std::is_same<T, U>::value.T::value`
        ///                                 ^~~~ reference

        return true;
    }

protected:
    bool mainFileOnly;
    clang::Sema& sema;
    clang::Preprocessor& pp;
    TemplateResolver& resolver;
    clang::SourceManager& srcMgr;
    clang::syntax::TokenBuffer& tokBuf;
    llvm::SmallVector<clang::Decl*> decls;
};

}  // namespace clice
