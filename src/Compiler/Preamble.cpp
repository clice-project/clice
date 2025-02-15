#include "Compiler/Preamble.h"
#include "clang/Lex/Lexer.h"

namespace clice {

std::uint32_t computePreambleBound(llvm::StringRef content) {
    clang::LangOptions langOpts;
    langOpts.CPlusPlus = true;
    langOpts.CPlusPlus26 = true;

    // Create a lexer starting at the beginning of the file. Note that we use a
    // "fake" file source location at offset 1 so that the lexer will track our
    // position within the file.
    auto beginLoc = clang::SourceLocation::getFromRawEncoding(1);
    clang::Lexer lexer(beginLoc, langOpts, content.begin(), content.begin(), content.end());

    bool isInDirective = false;

    clang::Token token;
    clang::Token end;

    while(true) {
        lexer.LexFromRawLexer(token);
        if(token.is(clang::tok::eof)) {
            break;
        }

        if(isInDirective) {
            /// If we are in a directive, we should skip the rest of the line.
            if(!token.isAtStartOfLine()) {
                end = token;
                continue;
            } else {
                isInDirective = false;
            }
        }

        if(token.isAtStartOfLine() && token.is(clang::tok::hash)) {
            /// If we encounter a `#` at the start of a line, it must be a directive.
            isInDirective = true;
            continue;
        } else if(token.isAtStartOfLine() && token.is(clang::tok::raw_identifier) &&
                  token.getRawIdentifier() == "module") {
            if(!lexer.LexFromRawLexer(token) && token.is(clang::tok::semi)) {
                /// If we encounter a `module` followed by a `;`, it must be
                /// a global module fragment. It should be a part of the preamble.
                end = token;
                continue;
            }
        }

        break;
    }

    if(auto endLocation = end.getLocation(); endLocation.isValid()) {
        return endLocation.getRawEncoding() - beginLoc.getRawEncoding() + end.getLength();
    } else {
        return 0;
    }
}

std::uint32_t computeBounds(CompilationParams& params) {
    return 0;
}

}  // namespace clice
