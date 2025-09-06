#pragma once

#include "Support/Format.h"
#include "llvm/ADT/SmallString.h"
#include "Support/Enum.h"

#include <expected>
#include <bitset>

namespace clice {

struct ParseGlobError : refl::Enum<ParseGlobError> {
    enum Kind : uint8_t {
        UnmatchedBrace,
        UnmatchedBracket,
        EmptyBrace,
        NestedBrace,
        TooManyBraceExpansions,
        InvalidRange,
        StrayBackslash,
        MultipleSlash,
        MultipleStar,
    };

    using Enum::Enum;
};

/// This class implements a glob pattern matcher to parse patterns to
/// watch relative to the base path.
///
/// Glob patterns can have the following syntax:
/// - `*` to match one or more characters in a path segment
/// - `?` to match on one character in a path segment
/// - `**` to match any number of path segments, including none
/// - `{}` to group conditions (e.g. `***.{ts,js}` matches all TypeScript
///   and JavaScript files)
/// - `[]` to declare a range of characters to match in a path segment
///   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
/// - `[!...]` to negate a range of characters to match in a path segment
///   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`,
///   but not `example.0`)
///
///   Note: Use only `/` for path segment seperator
class GlobPattern {
public:
    using GlobCharSet = std::bitset<std::numeric_limits<unsigned char>::max()>;

    /// \param pattern the pattern to match against
    /// \param max_subpattern_num if provided limit the number of allowed subpatterns
    ///               created from expanding braces otherwise disable brace expansion
    static auto create(llvm::StringRef pattern, std::optional<size_t> max_subpattern_num = {})
        -> std::expected<GlobPattern, ParseGlobError>;

    /// Returns true for glob pattern "*" or "**". Can be used to avoid expensive
    /// preparation/acquisition of the input for match().
    bool is_trivial_match_all() const;

    /// \returns \p true if \p str matches this glob pattern
    bool match(llvm::StringRef str) const;

private:
    /// GlobPattern is seperated into `Prefix + SubGlobPattern`
    std::string prefix;

    /// if prefix contains full path:
    ///
    /// xxx/yyy/*.c
    /// ~~~~~~^
    ///
    /// prefix contains incomplete path:
    /// xxx/yyy*.c
    /// ~~~~~~^
    ///
    bool prefix_at_seg_end = false;

    /// SubGlobPattern:
    /// Pattern `foo.{c,cpp,cppm}`
    /// -> extend to 3 SubGlobPatterns: `foo.c`, `foo.cpp`, `foo.cppm`
    struct SubPattern {

        /// \param pattern the pattern to match against
        static std::expected<SubPattern, ParseGlobError> create(llvm::StringRef pattern);

        /// \returns \p true if \p S matches this glob pattern
        bool match(llvm::StringRef str) const;

        /// Get pattern string
        llvm::StringRef str() const {
            return pattern.str();
        }

        struct Bracket {
            size_t next_offset;
            GlobCharSet bytes;
        };

        llvm::SmallVector<Bracket, 1> brackets;

        // GlobSegment devide patterm into segments by '/'
        // SubPattern:
        // **include/aaa/bbb/test[0-9].cc
        // ^~~~~~~~~^~~~^~~~^~~~~~~~~~~~~^
        //     1      2   3        4
        // Devided into 4 segments
        //
        // SubPattern:
        // **/include/aaa/bbb/test[0-9].h
        // ^~^~~~~~~~^~~~^~~~^~~~~~~~~~~~^
        //  1    2     3   4       5
        //  Devided into 5 segments
        struct GlobSegment {
            size_t start;
            size_t end;
        };

        llvm::SmallVector<GlobSegment> segments;

        llvm::SmallString<16> pattern;
    };

    llvm::SmallVector<SubPattern> sub_globs;
};

}  // namespace clice

template <>
struct std::formatter<clice::ParseGlobError> : std::formatter<llvm::StringRef> {
    template <typename FormatContext>
    auto format(clice::ParseGlobError& e, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", e.name());
    }
};

