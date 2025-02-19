#pragma once

#include "Basic/SourceCode.h"
#include "Compiler/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"

namespace clice {

/// A visitor class that extends clang::RecursiveASTVisitor to traverse
/// AST nodes with an additional filtering mechanism.
template <typename Derived>
class FilteredASTVisitor : public clang::RecursiveASTVisitor<Derived> {
private:
    using Base = clang::RecursiveASTVisitor<Derived>;

    bool filterable(clang::SourceRange range) {
        auto [begin, end] = range;

        /// FIXME: Most of implicit decls don't have valid source range. Is it possible
        /// that we want to visit them sometimes?
        if(begin.isInvalid() || end.isInvalid()) {
            return true;
        }

        if(begin == end) {
            /// We are only interested in expansion location.
            auto [fid, offset] = AST.getDecomposedLoc(AST.getExpansionLoc(begin));

            /// For builtin files, we don't want to visit them.
            if(AST.isBuiltinFile(fid)) {
                return true;
            }

            /// Filter out if the location is not in the interested file.
            if(interestedOnly) {
                auto interested = AST.getInterestedFile();
                if(fid != interested) {
                    return true;
                }

                if(targetRange && !targetRange->contains(offset)) {
                    return true;
                }
            }
        } else {
            auto [beginFID, beginOffset] = AST.getDecomposedLoc(AST.getExpansionLoc(begin));
            auto [endFID, endOffset] = AST.getDecomposedLoc(AST.getExpansionLoc(end));

            if(AST.isBuiltinFile(beginFID) || AST.isBuiltinFile(endFID)) {
                return true;
            }

            if(interestedOnly) {
                auto interested = AST.getInterestedFile();
                if(beginFID != interested && endFID != interested) {
                    return true;
                }

                if(targetRange && !targetRange->intersects({beginOffset, endOffset})) {
                    return true;
                }
            }
        }

        return false;
    }

    Derived& getDerived() {
        return static_cast<Derived&>(*this);
    }

public:
#define CHECK_DERIVED_IMPL(func)                                                                   \
    static_assert(std::same_as<decltype(&FilteredASTVisitor::func), decltype(&Derived::func)>,     \
                  "Derived class should not implement this method");

    bool TraverseDecl(clang::Decl* decl) {
        CHECK_DERIVED_IMPL(TraverseDecl);

        if(!decl) {
            return true;
        }

        if(llvm::isa<clang::TranslationUnitDecl>(decl)) {
            return Base::TraverseDecl(decl);
        }

        if(filterable(decl->getSourceRange()) || decl->isImplicit()) {
            return true;
        }

        /// We don't want to visit implicit instantiation.
        if(auto SD = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
            if(SD->getSpecializationKind() == clang::TSK_ImplicitInstantiation) {
                return true;
            }
        }

        if(auto SD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
            if(SD->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation) {
                return true;
            }
        }

        if(auto SD = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
            if(SD->getSpecializationKind() == clang::TSK_ImplicitInstantiation) {
                return true;
            }
        }

        return Base::TraverseDecl(decl);
    }

    bool TraverseStmt(clang::Stmt* stmt) {
        CHECK_DERIVED_IMPL(TraverseStmt);

        if(!stmt || filterable(stmt->getSourceRange())) {
            return true;
        }

        return Base::TraverseStmt(stmt);
    }

    /// FIXME: See https://github.com/llvm/llvm-project/issues/117687.
    bool TraverseAttributedStmt(clang::AttributedStmt* stmt) {
        CHECK_DERIVED_IMPL(TraverseAttributedStmt);

        if(!stmt || filterable(stmt->getSourceRange())) {
            return true;
        }

        for(auto attr: stmt->getAttrs()) {
            Base::TraverseAttr(const_cast<clang::Attr*>(attr));
        }

        return Base::TraverseAttributedStmt(stmt);
    }

    /// We don't want to node withou location information.
    constexpr bool TraverseType [[gnu::always_inline]] (clang::QualType) {
        CHECK_DERIVED_IMPL(TraverseType);
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        CHECK_DERIVED_IMPL(TraverseTypeLoc);

        if(!loc || filterable(loc.getSourceRange())) {
            return true;
        }

        /// FIXME: Workaround for `QualifiedTypeLoc`.
        if(auto QL = loc.getAs<clang::QualifiedTypeLoc>()) {
            return Base::TraverseTypeLoc(QL.getUnqualifiedLoc());
        }

        return Base::TraverseTypeLoc(loc);
    }

    bool TraverseAttr(clang::Attr* attr) {
        CHECK_DERIVED_IMPL(TraverseAttr);

        if(!attr || filterable(attr->getRange())) {
            return true;
        }

        return Base::TraverseAttr(attr);
    }

    /// We don't want to node withou location information.
    constexpr bool TraverseNestedNameSpecifier
        [[gnu::always_inline]] (clang::NestedNameSpecifier*) {
        CHECK_DERIVED_IMPL(TraverseNestedNameSpecifier);
        return true;
    }

    /// Note that `RecursiveASTVisitor` doesn't have `VisitNestedNameSpecifier`,
    /// it is our own implementation.
    bool VisitNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
        CHECK_DERIVED_IMPL(TraverseNestedNameSpecifierLoc);

        if(!loc || filterable(loc.getSourceRange())) {
            return true;
        }

        if(!getDerived().VisitNestedNameSpecifierLoc(loc)) {
            return false;
        }

        return Base::TraverseNestedNameSpecifierLoc(loc);
    }

    /// Note that `RecursiveASTVisitor` doesn't have `VisitNestedNameSpecifier`,
    /// it is our own implementation.
    bool VisitConceptReference(clang::ConceptReference* reference) {
        return true;
    }

    bool TraverseConceptReference(clang::ConceptReference* reference) {
        CHECK_DERIVED_IMPL(TraverseConceptReference);

        if(!reference || filterable(reference->getSourceRange())) {
            return true;
        }

        if(!getDerived().VisitConceptReference(reference)) {
            return false;
        }

        return Base::TraverseConceptReference(reference);
    }

#undef CHECK_DERIVED_IMPL

protected:
    FilteredASTVisitor(ASTInfo& AST,
                       bool interestedOnly,
                       std::optional<LocalSourceRange> targetRange) :
        AST(AST), interestedOnly(interestedOnly), targetRange(targetRange) {}

    ASTInfo& AST;
    bool interestedOnly = true;
    std::optional<LocalSourceRange> targetRange;
};

}  // namespace clice
