#pragma once

#include "SourceLocation.h"
#include "clang/Lex/Token.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice {

struct LocalSourceRange {
    /// The begin position offset to the source file.
    uint32_t begin = -1;

    /// The end position offset to the source file.
    uint32_t end = -1;

    constexpr bool operator== (const LocalSourceRange& other) const = default;

    constexpr bool contains(uint32_t offset) const {
        return offset >= begin && offset <= end;
    }

    constexpr bool intersects(const LocalSourceRange& other) const {
        return begin <= other.end && end >= other.begin;
    }

    constexpr bool valid() const {
        return begin != -1 && end != -1;
    }
};

/// Get the content of the file with the given file ID.
llvm::StringRef getFileContent(const clang::SourceManager& SM, clang::FileID fid);

/// Get the length of the token at the given location. All SourceLocation instances in the clang
/// AST originate from the start position of tokens, which helps reduce memory usage. When token
/// length information is needed, a simple lexing operation based on the start position can be
/// performed.
std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location);

/// Get the spelling of the token at the given location.
llvm::StringRef getTokenSpelling(const clang::SourceManager& SM, clang::SourceLocation location);

/// A fake location could be used to calculate the token location offset when lexer
/// runs in raw mode.
inline clang::SourceLocation fake_loc = clang::SourceLocation::getFromRawEncoding(1);

/// @brief Run `clang::Lexer` in raw mode and tokenize the content.
/// @param content The content to tokenize.
/// @param callback The callback to call for each token. Return false to break.
/// @param langOpts The language options to use. If not provided, lastest C++ standard is used.
void tokenize(llvm::StringRef content,
              llvm::unique_function<bool(const clang::Token&)> callback,
              bool ignoreComments = true,
              const clang::LangOptions* langOpts = nullptr);

struct Token {
    /// Whether this token is at the start of line.
    bool is_at_start_of_line = false;

    /// Whether this token is a preprocessor directive.
    bool is_preprocessor_directive = false;

    /// The kind of this token.
    clang::tok::TokenKind kind;

    /// The source range of this token.
    LocalSourceRange range;

    bool valid() {
        return range.valid();
    }

    llvm::StringRef name() {
        return clang::tok::getTokenName(kind);
    }

    llvm::StringRef text(llvm::StringRef content) {
        assert(range.valid() && "Invalid source range");
        return content.substr(range.begin, range.end - range.begin);
    }

    bool is_eof() {
        return kind == clang::tok::eof;
    }

    bool is_identifier() {
        return kind == clang::tok::identifier;
    }

    /// The tokens after the include diretive are regarded as
    /// a whole token, whose kind is `header_name`. For example
    /// `<iostream>` and `"test.h"` are both header name.
    bool is_header_name() {
        return kind == clang::tok::header_name;
    }
};

class Lexer {
public:
    Lexer(llvm::StringRef content,
          bool ignore_comments = true,
          const clang::LangOptions* lang_opts = nullptr);

    Lexer(const Lexer&) = delete;

    Lexer(Lexer&&) = delete;

    ~Lexer();

    void lex(Token& token);

    /// Get the token before this token without moving the lexer.
    Token last();

    /// Get the token after this token without moving the lexer.
    Token next();

    /// Advance the lexer and return the next token.
    Token advance();

private:
    /// Whether we are lexing the preprocessor directive.
    bool parse_preprocessor_directive = false;

    /// Whether we are lexing the header name.
    bool parse_header_name = false;

    /// The cache of last token.
    Token last_token;

    /// The cache of current token.
    Token current_token;

    /// The cache of next token.
    std::optional<Token> next_token;

    /// The lexed content.
    llvm::StringRef content;

    void* impl;
};

}  // namespace clice
