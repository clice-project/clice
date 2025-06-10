#include "Compiler/Preamble.h"
#include "AST/SourceCode.h"
#include "Support/Format.h"
#include "Support/Logger.h"

namespace clice {

std::uint32_t computePreambleBound(llvm::StringRef content) {
    auto result = computePreambleBounds(content);
    if(result.empty()) {
        return 0;
    } else {
        return result.back();
    }
}

std::vector<std::uint32_t> computePreambleBounds(llvm::StringRef content) {
    std::vector<std::uint32_t> result;

    /// The endLoc of the last token we iterate over.
    clang::SourceLocation end_loc;
    /// Whether the token is after `#`.
    bool isAfterHash = false;
    /// Whether the token is in the directive line.
    bool isInHeader = false;
    /// Whether the token is after `module`.
    bool isAfterModule = false;

    auto addResult = [&](const clang::SourceLocation end_loc) {
        if(end_loc.isInvalid()) {
            log::fatal("end_loc is invalid, but should be valid");
            return;
        }
        auto offset = end_loc.getRawEncoding() - fakeLoc.getRawEncoding();
        if(result.empty() || result.back() != offset) {
            result.emplace_back(offset);
        }
    };

    tokenize(content, [&](const clang::Token& token) {
        if(token.isAtStartOfLine()) {
            if(isInHeader) {
                /// If the last line is `#include`, add it to result.
                addResult(end_loc);
                isInHeader = false;
            }

            if(token.is(clang::tok::hash)) {
                /// If we encounter a `#` at the start of a line, it must be a directive.
                isAfterHash = true;
                return true;
            } else if(token.is(clang::tok::raw_identifier) &&
                      token.getRawIdentifier() == "module") {
                /// If we encounter a module keyword at the start of a line, it may be
                /// a module declaration or global module fragment.
                isAfterModule = true;
                return true;
            }

            /// If the first token is either `#` or `module`, We stop at here.
            return false;
        }

        if(isAfterHash) {
            isAfterHash = false;
            if(token.is(clang::tok::raw_identifier) && token.getRawIdentifier() == "include") {
                isInHeader = true;
            }
        }

        if(isAfterModule) {
            if(token.is(clang::tok::semi)) {
                isAfterModule = false;
            } else {
                /// If we meet a module declaration, directly return.
                return false;
            }
        }

        end_loc = token.getEndLoc();

        return true;
    });

    /// Add last location.
    if(end_loc.isInvalid()) {
        return std::vector<std::uint32_t>{};
    }

    addResult(end_loc);

    return result;
}

}  // namespace clice
