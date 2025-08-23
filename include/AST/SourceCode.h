#pragma once

#include <tuple>
#include "clang/Lex/Token.h"
#include "clang/Basic/SourceLocation.h"

namespace std {

template <>
struct tuple_size<clang::SourceRange> : std::integral_constant<std::size_t, 2> {};

template <>
struct tuple_element<0, clang::SourceRange> {
    using type = clang::SourceLocation;
};

template <>
struct tuple_element<1, clang::SourceRange> {
    using type = clang::SourceLocation;
};

}  // namespace std

namespace clang {

/// Through ADL, make `clang::SourceRange` could be destructured.
template <std::size_t I>
clang::SourceLocation get(clang::SourceRange range) {
    if constexpr(I == 0) {
        return range.getBegin();
    } else {
        return range.getEnd();
    }
}

}  // namespace clang

namespace clice {

struct LocalSourceRange {
    /// The begin position offset to the source file.
    uint32_t begin = -1;

    /// The end position offset to the source file.
    uint32_t end = -1;

    constexpr bool operator== (const LocalSourceRange& other) const = default;

    constexpr auto length() {
        return end - begin;
    }

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
        return kind == clang::tok::raw_identifier;
    }

    bool is_directive_hash() {
        return is_at_start_of_line && kind == clang::tok::hash;
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
          const clang::LangOptions* lang_opts = nullptr,
          bool ignore_end_of_directive = true);

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

    /// Advance the lexer until meet the specific kind token.
    Token advance_until(clang::tok::TokenKind kind);

private:
    /// If this is set to false, the lexer will emit tok::eod at the end
    /// of directive.
    bool ignore_end_of_directive = true;

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
