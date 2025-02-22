#include "AST/Selection.h"
#include "AST/Semantic.h"
#include "AST/Utility.h"
#include "Compiler/AST.h"
#include "Index/Shared.h"
#include "Support/Compare.h"
#include "Support/Ranges.h"
#include "Feature/Hover.h"

namespace clice::feature {

namespace {

std::vector<HoverItem> getHoverItems(ASTInfo& AST, const clang::NamedDecl* decl) {
    clang::ASTContext& Ctx = AST.context();
    std::vector<HoverItem> items;

    auto addItem = [&items](HoverItem::HoverKind kind, uint32_t value) {
        items.emplace_back(kind, llvm::Twine(value).str());
    };

    /// FIXME: Add other hover items.
    if(auto FD = llvm::dyn_cast<clang::FieldDecl>(decl)) {
        addItem(HoverItem::FieldIndex, FD->getFieldIndex());
        addItem(HoverItem::Offset, Ctx.getFieldOffset(FD));
        addItem(HoverItem::Size, Ctx.getTypeSizeInChars(FD->getType()).getQuantity());
        addItem(HoverItem::Align, Ctx.getTypeAlignInChars(FD->getType()).getQuantity());
        if(FD->isBitField()) {
            /// FIXME:
            /// addItem(HoverItem::BitWidth, FD->getBitWidthValue());
        }
    }

    return items;
}

std::string getDocument(ASTInfo& AST, const clang::NamedDecl* decl) {
    clang::ASTContext& Ctx = AST.context();
    const clang::RawComment* comment = Ctx.getRawCommentForAnyRedecl(decl);
    if(!comment) {
        return "";
    }

    return comment->getRawText(Ctx.getSourceManager()).str();
}

std::string getQualifier(ASTInfo& AST, const clang::NamedDecl* decl) {
    std::string result;
    llvm::raw_string_ostream os(result);
    decl->printNestedNameSpecifier(os);
    return result;
}

std::string getSourceCode(ASTInfo& AST, const clang::NamedDecl* decl) {
    clang::SourceRange range = decl->getSourceRange();
    auto& TB = AST.tokBuf();
    auto& SM = AST.srcMgr();
    auto tokens = TB.expandedTokens(range);
    /// FIXME: How to cut off the tokens?
    return "";
}

struct HoversStorage : Hovers {
    llvm::DenseMap<const void*, uint32_t> cache;

    void add(ASTInfo& AST, const clang::NamedDecl* decl, LocalSourceRange range) {
        auto [iter, success] = cache.try_emplace(decl, hovers.size());
        if(success) {
            hovers.emplace_back(hover(AST, decl));
        }
        occurrences.emplace_back(range, iter->second);
    }

    void sort() {
        std::vector<uint32_t> hoverMap(hovers.size());

        {
            std::vector<uint32_t> new2old(hovers.size());
            for(uint32_t i = 0; i < hovers.size(); ++i) {
                new2old[i] = i;
            }

            ranges::sort(views::zip(hovers, new2old), refl::less, [](const auto& element) {
                return std::get<0>(element);
            });

            for(uint32_t i = 0; i < hovers.size(); ++i) {
                hoverMap[new2old[i]] = i;
            }
        }

        for(auto& occurrence: occurrences) {
            occurrence.index = hoverMap[occurrence.index];
        }

        ranges::sort(occurrences, refl::less, [](const auto& item) { return item.range; });
    }
};

/// For index all hover information in the given AST.
class HoverCollector : public SemanticVisitor<HoverCollector> {
public:
    HoverCollector(ASTInfo& AST) : SemanticVisitor<HoverCollector>(AST, false) {}

    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        /// FIXME: Currently we only handle file loca1tion.
        if(location.isMacroID()) {
            return;
        }

        decl = normalize(decl);

        auto [fid, range] = AST.toLocalRange(location);
        auto& file = files[fid];
        file.add(AST, decl, range);
    }

    auto build() {
        index::Shared<Hovers> hovers;

        run();

        for(auto& [fid, storage]: files) {
            storage.sort();
            hovers[fid] = std::move(static_cast<Hovers&>(storage));
        }

        return hovers;
    }

private:
    index::Shared<HoversStorage> files;
};

}  // namespace

Hover hover(ASTInfo& AST, const clang::NamedDecl* decl) {
    return Hover{
        .kind = SymbolKind::from(decl),
        .name = getDeclName(decl),
        .items = getHoverItems(AST, decl),
        .document = getDocument(AST, decl),
        .qualifier = getQualifier(AST, decl),
        .source = getSourceCode(AST, decl),
    };
}

index::Shared<Hovers> indexHover(ASTInfo& AST) {
    HoverCollector collector(AST);
    return collector.build();
}

}  // namespace clice::feature
