#include "AST/SourceCode.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace clice {

llvm::StringRef getFileContent(const clang::SourceManager& SM, clang::FileID fid) {
    return SM.getBufferData(fid);
}

std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location) {
    return clang::Lexer::MeasureTokenLength(location, SM, {});
}

llvm::StringRef getTokenSpelling(const clang::SourceManager& SM, clang::SourceLocation location) {
    return llvm::StringRef(SM.getCharacterData(location), getTokenLength(SM, location));
}

void tokenize(llvm::StringRef content,
              llvm::unique_function<bool(const clang::Token&)> callback,
              bool ignoreComments,
              const clang::LangOptions* langOpts) {
    clang::LangOptions defaultLangOpts;
    defaultLangOpts.CPlusPlus = 1;
    defaultLangOpts.CPlusPlus26 = 1;
    defaultLangOpts.LineComment = !ignoreComments;

    clang::Lexer lexer(fake_loc,
                       langOpts ? *langOpts : defaultLangOpts,
                       content.begin(),
                       content.begin(),
                       content.end());
    lexer.SetCommentRetentionState(!ignoreComments);

    clang::Token token;
    while(true) {
        lexer.LexFromRawLexer(token);
        if(token.is(clang::tok::eof)) {
            break;
        }

        if(!callback(token)) {
            break;
        }
    }

    /// lexer.LexIncludeFilename()
}

Lexer::Lexer(llvm::StringRef content, bool ignore_comments, const clang::LangOptions* lang_opts) :
    content(content) {
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
        parse_preprocessor_directive = token.kind == clang::tok::hash;
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

}  // namespace clice
