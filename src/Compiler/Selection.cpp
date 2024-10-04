#include "Compiler/Selection.h"

namespace clice {

namespace {

class SelectionBuilder {
public:
    void push() {}

    void pop() {}

    template <typename Callback>
    bool hook(const clang::Stmt* stmt, const Callback& callback) {
        auto range = stmt->getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::TypeLoc& loc, const Callback& callback) {
        auto range = loc.getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::Attr* attr, const Callback& callback) {
        auto range = attr->getRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::Decl* decl, const Callback& callback) {
        auto range = decl->getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::NestedNameSpecifierLoc& NNS, const Callback& callback) {
        auto range = NNS.getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::TemplateArgumentLoc& argument, const Callback& callback) {
        auto range = argument.getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::CXXBaseSpecifier& base, const Callback& callback) {
        auto range = base.getSourceRange();
        return callback();
    }

    template <typename Callback>
    bool hook(const clang::CXXCtorInitializer* init, const Callback& callback) {
        auto range = init->getSourceRange();
        return callback();
    }
};

class SelectionCollector : public clang::RecursiveASTVisitor<SelectionCollector> {
public:
    SelectionCollector(SelectionBuilder& builder) : builder(builder) {}

    using Base = clang::RecursiveASTVisitor<SelectionCollector>;

    bool TraverseStmt(clang::Stmt* stmt) {
        return builder.hook(stmt, [&] {
            return Base::TraverseStmt(stmt);
        });
    }

    /// we don't care about the type without location information.
    /// so just skip all its children.
    bool TraverseType(clang::QualType type) {
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        return builder.hook(loc, [&] {
            return Base::TraverseTypeLoc(loc);
        });
    }

    bool TraverseAttr(clang::Attr* attr) {
        return builder.hook(attr, [&] {
            return Base::TraverseAttr(attr);
        });
    }

    bool TraverseDecl(clang::Decl* decl) {
        return builder.hook(decl, [&] {
            return Base::TraverseDecl(decl);
        });
    }

    /// we don't care about the name without location information.
    /// so just skip all its children.
    bool TraverseNestedNameSpecifier(clang::NestedNameSpecifier* NNS) {
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        return builder.hook(NNS, [&] {
            return Base::TraverseNestedNameSpecifierLoc(NNS);
        });
    }

    bool TraverseTemplateArgumentLoc(const clang::TemplateArgumentLoc& argument) {
        return builder.hook(argument, [&] {
            return Base::TraverseTemplateArgumentLoc(argument);
        });
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return builder.hook(base, [&] {
            return Base::TraverseCXXBaseSpecifier(base);
        });
    }

    bool TraverseConstructorInitializer(clang::CXXCtorInitializer* init) {
        return builder.hook(init, [&] {
            return Base::TraverseConstructorInitializer(init);
        });
    }

    // FIXME: figure out concept in clang AST.
    bool TraverseConceptReference(clang::ConceptReference* concept_) {
        return true;
    }

private:
    SelectionBuilder& builder;
};

}  // namespace

}  // namespace clice
