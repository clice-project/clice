#include "Feature/InlayHint.h"

namespace clice {

namespace {

/// Compute inlay hints for a document in given range and config.
struct InlayHintCollector : public clang::RecursiveASTVisitor<InlayHintCollector> {

    /// The source manager of the translation unit.
    const clang::SourceManager* src;

    /// The config of inlay hints collector.
    const config::InlayHintConfig* config;

    /// The restrict range of request.
    clang::SourceRange limit;

    /// The result of inlay hints.
    proto::InlayHintsResult result;

    /// Do not produce inlay hints if either range ends is not within the main file.
    bool needFilter(clang::SourceRange range) {
        // skip invalid range or not in main file
        if(range.isInvalid())
            return true;

        if(!src->isInMainFile(range.getBegin()) || !src->isInMainFile(range.getEnd()))
            return true;

        // not involved in restrict range
        if(range.getEnd() < limit.getBegin() || range.getBegin() > limit.getEnd())
            return true;

        return false;
    }
};

}  // namespace

namespace feature {

json::Value inlayHintCapability(json::Value InlayHintClientCapabilities) {
    return {};
}

/// Compute inlay hints for a document in given range and config.
proto::InlayHintsResult inlayHints(proto::Range range, ASTInfo& ast,
                                   const config::InlayHintConfig* config) {

    clang::SourceManager* src = &ast.srcMgr();
    clang::FileID main = src->getMainFileID();

    /// FIXME:
    /// Take 0-0 based Lsp Location from `range` parameter and convert it to clang 1-1 based
    /// source location.
    clang::SourceRange fixedRange;  // = range...

    // In default, use the whole main file as the restrict range.
    if(fixedRange.isInvalid())
        fixedRange = {src->getLocForStartOfFile(main), src->getLocForEndOfFile(main)};

    InlayHintCollector collector{
        .src = src,
        .config = config,
        .limit = fixedRange,
    };

    collector.TraverseTranslationUnitDecl(ast.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
