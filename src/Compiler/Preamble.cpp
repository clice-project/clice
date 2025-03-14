#include "Compiler/Preamble.h"
#include "Basic/SourceCode.h"
#include "Support/Format.h"

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

    /// The last token we iterate over.
    clang::Token end;
    /// Whether the token is after `#`.
    bool isAfterHash = false;
    /// Whether the token is in the directive line.
    bool isInHeader = false;
    /// Whether the token is after `module`.
    bool isAfterModule = false;

    auto addResult = [&](const clang::Token& token) {
        auto offset =
            token.getLocation().getRawEncoding() - fakeLoc.getRawEncoding() + token.getLength();
        if(result.empty() || result.back() != offset) {
            result.emplace_back(offset);
        }
    };

    tokenize(content, [&](const clang::Token& token) {
        if(token.isAtStartOfLine()) {
            if(isInHeader) {
                /// If the last line is `#include`, add it to result.
                addResult(end);
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

        end = token;

        return true;
    });

    /// Add last location.
    if(end.getLocation().isInvalid()) {
        return std::vector<std::uint32_t>{};
    }

    addResult(end);

    return result;
}

}  // namespace clice
