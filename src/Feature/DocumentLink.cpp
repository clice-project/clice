#include "Compiler/AST.h"
#include "Feature/DocumentLink.h"

namespace clice::feature {

namespace {}

DocumentLinkResult documentLink(ASTInfo& AST);

index::Shared<DocumentLinkResult> indexDocumentLink(ASTInfo& AST) {
    index::Shared<DocumentLinkResult> result;

    for(auto& [fid, diretives]: AST.directives()) {
        for(auto& include: diretives.includes) {
            auto path = AST.getFilePath(include.fid);
            result[fid].emplace_back(DocumentLink{.file = path.str()});
        }
    }

    return result;
}

}  // namespace clice::feature
