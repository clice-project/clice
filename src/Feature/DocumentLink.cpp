#include "Compiler/AST.h"
#include "Feature/DocumentLink.h"

namespace clice::feature {

namespace {}

DocumentLinkResult documentLink(ASTInfo& AST);

index::Shared<DocumentLinkResult> indexDocumentLink(ASTInfo& AST) {
    index::Shared<DocumentLinkResult> result;

    for(auto& [fid, diretives]: AST.directives()) {
        for(auto& include: diretives.includes) {
            auto [_, range] = AST.toLocalRange(include.fileNameRange);
            auto path = AST.getFilePath(include.fid);
            result[fid].emplace_back(range, path.str());
        }

        auto content = AST.getFileContent(fid);
        for(auto& hasInclude: diretives.hasIncludes) {
            /// If the include path is empty, skip it.
            if(hasInclude.fid.isValid()) {
                continue;
            }

            auto location = hasInclude.location;
            auto [_, offset] = AST.getDecomposedLoc(location);

            auto subContent = content.substr(offset);

            bool isAngled = true;
            tokenize(subContent, [](const clang::Token& token) { return true; });
        }
    }

    return result;
}

}  // namespace clice::feature
