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

using DocumentLinks = std::vector<DocumentLink>;

/// Generate document link for main file.
DocumentLinks documentLinks(ASTInfo& AST);

/// Generate document link for all source file.
index::Shared<DocumentLinks> indexDocumentLink(ASTInfo& AST);

}  // namespace clice::feature

