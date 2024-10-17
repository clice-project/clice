#include <Index/Index.h>
#include <clang/Index/USRGeneration.h>

namespace clice {

namespace {

class SymbolBuilder {
public:
    CSIF dump() {
        CSIF csif;
        csif.version = "0.0.1";
        csif.language = "C++";
        csif.symbols = symbols;
        csif.occurrences = occurrences;
        return csif;
    }

    std::size_t addSymbolID(const clang::Decl* decl) {
        auto iter = cache.find(decl);
        if(iter != cache.end()) {
            return iter->second;
        }

        // Generate and save USR.
        llvm::SmallString<128> USR;
        clang::index::generateUSRForDecl(decl, USR);
        saver.save(USR.str());

        auto index = symbols.size();
        cache[decl] = index;
        return index;
    }

    SymbolBuilder& addSymbol(const clang::Decl* decl) {
        return *this;
    }

    SymbolBuilder& addRelation(const clang::Decl* from, const clang::Decl* to, Role role) {
        // FIXME:
        return *this;
    }

    // TODO:
    SymbolBuilder& addOccurrence() {
        return *this;
    }

    template <typename Node, typename Callback>
    bool hook(Node* node, const Callback& callback) {
        return true;
    }

private:
    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
    std::vector<std::vector<Relation>> relations;
    llvm::DenseMap<const clang::Decl*, std::size_t> cache;

    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver{allocator};
};

class SymbolCollector : public clang::RecursiveASTVisitor<SymbolCollector> {
    using Base = clang::RecursiveASTVisitor<SymbolCollector>;

public:
    SymbolCollector(SymbolBuilder& builder) : builder(builder) {}

    bool TraverseDecl(clang::Decl* decl) {
        /// `TranslationUnitDecl` has invalid location information.
        /// So we process it separately.
        if(llvm::isa_and_nonnull<clang::TranslationUnitDecl>(decl)) {
            return Base::TraverseDecl(decl);
        }

        // TODO: generate SymbolID for every decl.
        // Distinguish linkage, for no or internal linkage.
        // For them, relation lookup is only occurred in current TU.

        return builder.hook(decl, [&] {
            return Base::TraverseDecl(decl);
        });
    }

    // FIXME: check DeclRefExpr, MemberExpr, etc.

    bool TraverseStmt(clang::Stmt* stmt) {
        return builder.hook(stmt, [&] {
            return Base::TraverseStmt(stmt);
        });
    }

    bool TraverseAttr(clang::Attr* attr) {
        return builder.hook(attr, [&] {
            return Base::TraverseAttr(attr);
        });
    }

    /// we don't care about the node without location information, so skip them.
    constexpr bool shouldWalkTypesOfTypeLocs [[gnu::const]] () {
        return false;
    }

    constexpr bool TraverseType [[gnu::const]] (clang::QualType) {
        return true;
    }

    constexpr bool TraverseNestedNameSpecifier [[gnu::const]] (clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        /// clang currently doesn't record any information for `QualifiedTypeLoc`.
        /// It has same location with its inner type. So we just ignore it.
        if(auto QTL = loc.getAs<clang::QualifiedTypeLoc>()) {
            return TraverseTypeLoc(QTL.getUnqualifiedLoc());
        }

        return builder.hook(&loc, [&] {
            return Base::TraverseTypeLoc(loc);
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

    bool VisiDeclRefExpr(const clang::DeclRefExpr* expr) {
        auto decl = expr->getDecl();

        auto range = expr->getNameInfo().getSourceRange();
        // TODO: add occurrence.
        return true;
    }

    bool VisitMemberExpr(const clang::MemberExpr* expr) {
        auto decl = expr->getMemberDecl();

        auto range = expr->getMemberLoc();
        return true;
    }

    bool VisitDependentCXXScopeMemberExpr(const clang::DependentScopeDeclRefExpr* expr) {
        // TODO: use TemplateResolver here.
        auto decl = expr->getQualifier();
        auto range = expr->getNameInfo().getSourceRange();
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        // TODO: use TemplateResolver here.
        auto range = NNS.getSourceRange();
        auto range2 = NNS.getLocalSourceRange();

        return Base::TraverseNestedNameSpecifierLoc(NNS);
    }

#define VISIT_TYOELOC(name) bool Visit##name(clang::name loc)

    // TODO: ... add occurrence and relation.

    VISIT_TYOELOC(BuiltinTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(RecordTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(EnumTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(ElaboratedTypeLoc) {
        auto loc1 = loc.getElaboratedKeywordLoc();
        // render keyword.
    }

    VISIT_TYOELOC(TypedefTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(TemplateSpecializationTypeLoc) {
        auto range = loc.getTemplateNameLoc();
        return true;
    }

    // TODO. TemplateTypeParmTypeLoc, UsingType, AttributedTypeLoc, MacroQualifiedTypeLoc, ParenType, AdjustedTypeLoc
    // MemberPointerTypeLoc

private:
    SymbolBuilder& builder;
};

}  // namespace

CSIF index(clang::ASTContext& context) {
    SymbolBuilder builder;
    SymbolCollector collector(builder);
    collector.TraverseAST(context);
    return builder.dump();
};

}  // namespace clice
