#pragma once

#include <vector>

#include "AST/SourceCode.h"
#include "Index/Shared.h"

namespace clice::feature {

struct DocumentLink {
    /// The range of the whole link.
    LocalSourceRange range;

    /// The target string path.
    std::string file;
};

using DocumentLinkResult = std::vector<DocumentLink>;

/// Generate document link for main file.
DocumentLinkResult documentLink(ASTInfo& AST);

/// Generate document link for all source file.
index::Shared<DocumentLinkResult> indexDocumentLink(ASTInfo& AST);

}  // namespace clice::feature

