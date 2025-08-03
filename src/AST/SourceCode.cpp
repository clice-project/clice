#include "AST/SourceCode.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace clice {

std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location) {
    return clang::Lexer::MeasureTokenLength(location, SM, {});
}

/// A fake location could be used to calculate the token location offset when lexer
/// runs in raw mode.
inline static clang::SourceLocation fake_loc = clang::SourceLocation::getFromRawEncoding(1);

Lexer::Lexer(llvm::StringRef content,
             bool ignore_comments,
             const clang::LangOptions* lang_opts,
             bool ignore_end_of_directive) :
    content(content), ignore_end_of_directive(ignore_end_of_directive) {

    static clang::LangOptions default_opts;
    auto lexer = new clang::Lexer(fake_loc,
                                  lang_opts ? *lang_opts : default_opts,
                                  content.begin(),
                                  content.begin(),
                                  content.end());
    lexer->SetCommentRetentionState(!ignore_comments);
    impl = lexer;
}

Lexer::~Lexer() {
    delete static_cast<clang::Lexer*>(impl);
}

void Lexer::lex(Token& token) {
    auto lexer = static_cast<clang::Lexer*>(impl);

    clang::Token raw_token;

    if(parse_header_name) {
        lexer->LexIncludeFilename(raw_token);
    } else {
        lexer->LexFromRawLexer(raw_token);
    }

    token.kind = raw_token.getKind();
    token.is_at_start_of_line = raw_token.isAtStartOfLine();
    token.is_preprocessor_directive = parse_preprocessor_directive;

    auto offset = raw_token.getLocation().getRawEncoding() - fake_loc.getRawEncoding();
    token.range = LocalSourceRange{offset, offset + raw_token.getLength()};

    if(token.is_at_start_of_line) {
        if(token.kind == clang::tok::hash) {
            parse_preprocessor_directive = true;
            lexer->setParsingPreprocessorDirective(true);
        }

        parse_header_name = false;
    } else if(parse_preprocessor_directive) {
        /// Preprocessor directive token only have one.
        parse_preprocessor_directive = false;

        parse_header_name = token.text(content) == "include";
    }
}

Token Lexer::last() {
    return last_token;
}

Token Lexer::next() {
    if(!next_token) {
        Token token;
        lex(token);
        next_token.emplace(token);
    }

    return *next_token;
}

Token Lexer::advance() {
    last_token = current_token;

    if(next_token) {
        current_token = *next_token;
        next_token.reset();
    } else {
        Token token;
        lex(token);
        current_token = token;
    }

    return current_token;
}

Token Lexer::advance_until(clang::tok::TokenKind kind) {
    while(true) {
        auto token = advance();
        if(token.kind == kind || token.is_eof()) {
            return token;
        }
    }
}

}  // namespace clice
