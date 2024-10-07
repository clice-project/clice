#include "Compiler/Selection.h"

namespace clice {

namespace {

class SelectionBuilder {
public:
    SelectionBuilder(clang::SourceRange input, clang::ASTContext& context) : input(input), context(context) {}

    void push() {}

    void pop() {}

    template <typename Node>
    bool isSkippable(const Node* node) {
        if constexpr(std::is_same_v<Node, clang::Decl>) {
            if(llvm::dyn_cast<clang::TranslationUnitDecl>(node)) {
                return false;
            }
        }

        clang::SourceRange range;
        if constexpr(std::is_base_of_v<Node, clang::Attr>) {
            range = node->getRange();
        } else {
            range = node->getSourceRange();
        }

        if(range.isInvalid()) {
            return true;
        }

        range.dump(context.getSourceManager());
        return input.getBegin() > range.getEnd() || input.getEnd() < range.getBegin();
    }

    template <typename Node, typename Callback>
    bool hook(const Node* node, const Callback& callback) {
        if(isSkippable(node)) {
            return true;
        }

        return callback();
    }

private:
    clang::SourceRange input;
    clang::ASTContext& context;
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
        return builder.hook(&loc, [&] {
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
        return builder.hook(&NNS, [&] {
            return Base::TraverseNestedNameSpecifierLoc(NNS);
        });
    }

    bool TraverseTemplateArgumentLoc(const clang::TemplateArgumentLoc& argument) {
        return builder.hook(&argument, [&] {
            return Base::TraverseTemplateArgumentLoc(argument);
        });
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return builder.hook(&base, [&] {
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

SelectionTree::SelectionTree(clang::ASTContext& context,
                             const clang::syntax::TokenBuffer& tokens,
                             clang::SourceLocation begin,
                             clang::SourceLocation end) {

    SelectionBuilder builder({begin, end}, context);
    SelectionCollector collector(builder);
    collector.TraverseAST(context);
    // context.getTranslationUnitDecl()->dump();
}

}  // namespace clice
