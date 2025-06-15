#include "AST/FilterASTVisitor.h"

namespace clice {

bool RAVFileter::filterable(clang::SourceRange range) const {
    auto [begin, end] = range;

    /// FIXME: Most of implicit decls don't have valid source range. Is it possible
    /// that we want to visit them sometimes?
    if(begin.isInvalid() || end.isInvalid()) {
        return true;
    }

    if(begin == end) {
        /// We are only interested in expansion location.
        auto [fid, offset] = unit.getDecomposedLoc(unit.getExpansionLoc(begin));

        /// For builtin files, we don't want to visit them.
        if(unit.isBuiltinFile(fid)) {
            return true;
        }

        /// Filter out if the location is not in the interested file.
        if(interestedOnly) {
            auto interested = unit.getInterestedFile();
            if(fid != interested) {
                return true;
            }

            if(limit && !limit->contains(offset)) {
                return true;
            }
        }
    } else {
        auto [beginFID, beginOffset] = unit.getDecomposedLoc(unit.getExpansionLoc(begin));
        auto [endFID, endOffset] = unit.getDecomposedLoc(unit.getExpansionLoc(end));

        if(unit.isBuiltinFile(beginFID) || unit.isBuiltinFile(endFID)) {
            return true;
        }

        if(interestedOnly) {
            auto interested = unit.getInterestedFile();
            if(beginFID != interested && endFID != interested) {
                return true;
            }

            if(limit && !limit->intersects({beginOffset, endOffset})) {
                return true;
            }
        }
    }

    return false;
}

}  // namespace clice
