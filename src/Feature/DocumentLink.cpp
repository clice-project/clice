#include "Compiler/AST.h"
#include "Feature/DocumentLink.h"
#include "Support/Ranges.h"
#include "Support/Compare.h"

namespace clice::feature {

namespace {}

DocumentLinkResult documentLink(ASTInfo& AST);

index::Shared<DocumentLinkResult> indexDocumentLink(ASTInfo& AST) {
    index::Shared<DocumentLinkResult> result;

    for(auto& [fid, diretives]: AST.directives()) {
        for(auto& include: diretives.includes) {
            auto [_, range] = AST.toLocalRange(include.fileNameRange);
            result[fid].emplace_back(range, AST.getFilePath(include.fid).str());
        }

        auto content = AST.getFileContent(fid);
        for(auto& hasInclude: diretives.hasIncludes) {
            /// If the include path is empty, skip it.
            if(hasInclude.fid.isInvalid()) {
                continue;
            }

            auto location = hasInclude.location;
            auto [_, offset] = AST.getDecomposedLoc(location);

            auto subContent = content.substr(offset);

            bool isFirst = true;
            std::uint32_t endOffset = offset;
            tokenize(subContent, [&](const clang::Token& token) {
                if(token.is(clang::tok::r_paren) || (!isFirst && token.isAtStartOfLine())) {
                    return false;
                }

                if(isFirst) {
                    isFirst = false;
                }

                endOffset = offset + token.getEndLoc().getRawEncoding() - fakeLoc.getRawEncoding();
                return true;
            });

            result[fid].emplace_back(LocalSourceRange{offset, endOffset},
                                     AST.getFilePath(hasInclude.fid).str());
        }
    }

    for(auto& [_, links]: result) {
        ranges::sort(links, refl::less);
    }

    return result;
}

}  // namespace clice::feature
