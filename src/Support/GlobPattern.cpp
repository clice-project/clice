#include "llvm/ADT/STLExtras.h"
#include "Support/GlobPattern.h"

namespace clice {

namespace {

/// Expands character ranges and returns a bitmap.
/// For example, "a-cf-hz" is expanded to "abcfghz".
std::expected<GlobPattern::GlobCharSet, ParseGlobError> expand(llvm::StringRef str,
                                                               llvm::StringRef original) {
    using enum ParseGlobError::Kind;

    GlobPattern::GlobCharSet charset;
    for(size_t i = 0, e = str.size(); i < e; ++i) {
        switch(str[i]) {
            case '\\': {
                ++i;
                if(i == e) {
                    return std::unexpected(StrayBackslash);
                }
                if(str[i] != '/') {
                    charset.set(str[i]);
                }
                break;
            }

            case '-': {
                if(i == 0 || i + 1 == e) {
                    charset.set('-');
                    break;
                }
                char c_begin = str[i - 1];
                char c_end = str[i + 1];
                ++i;
                if(c_end == '\\') {
                    ++i;
                    if(str[i] == e) {
                        return std::unexpected(StrayBackslash);
                    }
                    c_end = str[i];
                }
                if(c_begin > c_end) {
                    return std::unexpected(InvalidRange);
                }
                for(char c = c_begin; c <= c_end; ++c) {
                    if(c != '/') {
                        charset.set(c);
                    }
                }
                break;
            }

            default: {
                if(str[i] != '/') {
                    charset.set(str[i]);
                }
            }
        }
    }

    return charset;
}

/// Identify brace expansions in str and return the list of patterns they expand into.
std::expected<llvm::SmallVector<std::string>, ParseGlobError>
    parse_brace_expansion(llvm::StringRef str, std::optional<size_t> max_subpattern_num) {
    using enum ParseGlobError::Kind;

    llvm::SmallVector<std::string> subpatterns = {str.str()};
    if(!max_subpattern_num || !str.contains('{')) {
        return subpatterns;
    }

    struct BraceExpansion {
        size_t start;
        size_t length;
        llvm::SmallVector<llvm::StringRef, 2> terms;
    };

    llvm::SmallVector<BraceExpansion, 0> brace_expansions;

    BraceExpansion* current_be = nullptr;
    size_t term_begin = 0;
    for(size_t i = 0, e = str.size(); i != e; ++i) {
        if(str[i] == '[') {
            ++i;
            if(str[i] == ']') {
                ++i;
            }
            while(i != e && str[i] != ']') {
                if(str[i] == '\\') {
                    if(i == e) {
                        return std::unexpected(UnmatchedBracket);
                    }
                    ++i;
                }
                ++i;
            }
            if(i == e) {
                return std::unexpected(UnmatchedBracket);
            }
        } else if(str[i] == '{') {
            if(current_be) {
                return std::unexpected(NestedBrace);
            }
            current_be = &brace_expansions.emplace_back();
            current_be->start = i;
            term_begin = i + 1;
        } else if(str[i] == ',') {
            if(!current_be) {
                continue;
            }
            current_be->terms.push_back(str.substr(term_begin, i - term_begin));
            term_begin = i + 1;
        } else if(str[i] == '}') {
            if(!current_be) {
                continue;
            }
            if(current_be->terms.empty() && i - term_begin == 0) {
                return std::unexpected(EmptyBrace);
            }
            current_be->terms.push_back(str.substr(term_begin, i - term_begin));
            current_be->length = i - current_be->start + 1;
            current_be = nullptr;
        } else if(str[i] == '\\') {
            ++i;
            if(i == e) {
                return std::unexpected(StrayBackslash);
            }
        }
    }

    if(current_be) {
        return std::unexpected(UnmatchedBrace);
    }

    size_t subpattern_num = 1;
    for(auto& be: brace_expansions) {
        if(subpattern_num > std::numeric_limits<size_t>::max() / be.terms.size()) {
            subpattern_num = std::numeric_limits<size_t>::max();
            break;
        }
        subpattern_num *= be.terms.size();
    }

    if(subpattern_num > *max_subpattern_num) {
        return std::unexpected(TooManyBraceExpansions);
    }

    for(auto& be: reverse(brace_expansions)) {
        llvm::SmallVector<std::string> orig_sub_patterns;
        std::swap(subpatterns, orig_sub_patterns);
        for(llvm::StringRef term: be.terms) {
            for(llvm::StringRef orig: orig_sub_patterns) {
                subpatterns.emplace_back(orig).replace(be.start, be.length, term);
            }
        }
    }

    return subpatterns;
}

}  // namespace

auto GlobPattern::create(llvm::StringRef pattern, std::optional<size_t> max_subpattern_num)
    -> std::expected<GlobPattern, ParseGlobError> {
    using enum ParseGlobError::Kind;

    // Store the prefix that does not contain any metacharacter.
    GlobPattern pat;
    size_t prefix_size = pattern.find_first_of("?*[{\\");
    if(prefix_size == std::string::npos) {
        pat.prefix = pattern.substr(0, prefix_size).str();
        // check if there is multiple `/` in prefix
        size_t last_slash = 0;
        size_t size = pat.prefix.size();
        for(size_t i = 0; i < size; ++i) {
            if(pattern[i] == '/' && i - last_slash == 1) {
                return std::unexpected(MultipleSlash);
            }
            last_slash = i;
        }
        return pat;
    }
    if(prefix_size != 0 && pattern[prefix_size - 1] == '/') {
        pat.prefix_at_seg_end = true;
        --prefix_size;
    }
    pat.prefix = pattern.substr(0, prefix_size).str();
    // check if there is multiple `/` in prefix
    size_t last_slash = 0;
    size_t size = pat.prefix.size();
    for(size_t i = 0; i < size; ++i) {
        if(pattern[i] == '/' && i - last_slash == 1) {
            return std::unexpected(MultipleSlash);
        }
        last_slash = i;
    }
    pattern = pattern.substr(pat.prefix_at_seg_end ? prefix_size + 1 : prefix_size);

    llvm::SmallVector<std::string> sub_patterns;
    if(auto expansion = parse_brace_expansion(pattern, max_subpattern_num); expansion.has_value()) {
        sub_patterns = expansion.value();
    } else {
        return std::unexpected(expansion.error());
    }

    for(auto sub_pat: sub_patterns) {
        if(auto res = SubPattern::create(sub_pat); res.has_value()) {
            pat.sub_globs.push_back(res.value());
        } else {
            return std::unexpected(res.error());
        }
    }

    return pat;
}

std::expected<GlobPattern::SubPattern, ParseGlobError>
    GlobPattern::SubPattern::create(llvm::StringRef pattern) {
    using enum ParseGlobError::Kind;

    llvm::SmallVector<Bracket> brackets;
    llvm::SmallVector<GlobSegment> segments;
    GlobSegment* current = &segments.emplace_back();

    current->start = 0;
    // Parse brackets.
    size_t e = pattern.size();
    for(size_t i = 0; i < e; ++i) {
        if(!current) {
            current = &segments.emplace_back();
            current->start = i;
        }
        if(pattern[i] == '[') {
            ++i;
            size_t j = i;
            if(pattern[j] == ']') {
                // ']' is allowed as the first character of a character class. '[]'
                // is invalid. So, just skip the first character.
                ++j;
            }
            while(j != e && pattern[j] != ']') {
                // Check if there is invalid escape char
                ++j;
                if(pattern[j - 1] == '\\') {
                    if(j == e) {
                        // Reach here in such case:
                        // [a-zA-Z\]
                        // ~~~~~~~~^
                        return std::unexpected(StrayBackslash);
                    }
                    ++j;
                }
            }
            if(j == e) {
                return std::unexpected(UnmatchedBracket);
            }
            llvm::StringRef chars = pattern.substr(i, j - i);
            bool invert = pattern[i] == '^' || pattern[i] == '!';
            auto bv = invert ? expand(chars.substr(1), pattern) : expand(chars, pattern);
            if(!bv.has_value()) {
                return std::unexpected(bv.error());
            }
            if(invert) {
                bv->flip();
            }
            brackets.emplace_back(j + 1, bv.value());
            i = j;
        } else if(pattern[i] == '\\') {
            if(++i == e) {
                return std::unexpected(StrayBackslash);
            }
        } else if(pattern[i] == '/') {
            if(i - current->start == 1) {
                return std::unexpected(MultipleSlash);
            }
            current->end = i;
            current = nullptr;
        } else if(pattern[i] == '*') {
            // check if there is multiple `*`
            if(i + 1 < e - 1 && pattern[i + 1] == '*' && pattern[i + 2] == '*') {
                return std::unexpected(MultipleStar);
            }
        }
    }

    if(current) {
        current->end = e;
        current = nullptr;
    }

    return SubPattern{
        .brackets = std::move(brackets),
        .segments = std::move(segments),
        .pattern = pattern,
    };
}

bool GlobPattern::match(llvm::StringRef str) const {
    if(!str.consume_front(prefix)) {
        return false;
    }

    if(str.empty() && sub_globs.empty()) {
        return true;
    }

    if(!str.empty() && prefix_at_seg_end) {
        if(str[0] != '/') {
            return false;
        }
        str = str.substr(1);
    }

    for(const auto& glob: sub_globs) {
        if(glob.match(str)) {
            return true;
        }
    }
    return false;
}

bool GlobPattern::SubPattern::match(llvm::StringRef str) const {
    // p: Current position in the pattern
    // s: Current position in the String
    // b: Current processed bracket num
    // s_start: String start
    // s_end: String end
    // p_start: Pattern start
    // p_end: Pattern end
    // seg_start: Position of the start of current segment
    // s_end: Position of the end of current segment
    // b: Current matched bracket
    // current_glob_seg: Index of glob patterns
    // wild_mode: Whether wildcards are allowed to cross segments
    const char* s = str.data();
    const char* const s_start = s;
    const char* const s_end = s + str.size();
    const char* p = pattern.data();
    const char* seg_start = p;
    const char* const p_start = p;
    const char* const p_end = p + pattern.size();
    const char* seg_end = p + segments[0].end;
    size_t b = 0;
    size_t current_glob_seg = 0;
    bool wild_mode = false;

    // Status for backtracing
    struct BacktraceStat {
        size_t b;
        size_t glob_seg;
        bool wild_mode;
        const char* p;
        const char* s;
        const char* seg_end;
        const char* seg_start;
    };

    llvm::SmallVector<BacktraceStat, 6> backtrace_stack;
    const size_t seg_num = segments.size();

    auto save_stat =
        [&backtrace_stack, &b, &current_glob_seg, &wild_mode, &p, &s, &seg_end, &seg_start]() {
            backtrace_stack.push_back({.b = b,
                                       .glob_seg = current_glob_seg,
                                       .wild_mode = wild_mode,
                                       .p = p,
                                       .s = s,
                                       .seg_end = seg_end,
                                       .seg_start = seg_start});
        };

    while(current_glob_seg < seg_num) {

        if(s == s_end) {
            // Return true if all pattern characters are processed or only
            // '*' or '/' characters remain
            return pattern.find_first_not_of("*/", p - pattern.data()) == std::string::npos;
        }

        if(p != seg_end) {
            switch(*p) {
                case '*': {
                    if(p + 1 != p_end && *(p + 1) == '*') {
                        // Met '**'
                        p += 2;
                        wild_mode = true;
                        // Consume extra '*'
                        while(p != p_end && (*p == '*' || *p == '/')) {
                            if(*p == '/') {
                                if(current_glob_seg + 1 == seg_num) {
                                    return true;
                                }
                                ++current_glob_seg;
                                seg_start = p_start + segments[current_glob_seg].start;
                                seg_end = p_start + segments[current_glob_seg].end;
                            }
                            ++p;
                        }
                        if(p == seg_end) {
                            // '**' at segment end
                            if(current_glob_seg + 1 == seg_num) {
                                // '**' at pattern end
                                return true;
                            }

                            // Goto next segment
                            ++current_glob_seg;
                            while(s != s_end && *s == '/') {
                                ++s;
                            }
                            p = p_start + segments[current_glob_seg].start;
                            seg_start = p;
                            seg_end = p_start + segments[current_glob_seg].end;
                        }
                        save_stat();
                    } else {
                        // Met '*'
                        ++p;
                        wild_mode = false;
                        if(p == seg_end) {
                            while(s != s_end && *s != '/') {
                                ++s;
                            }
                            if(s == s_end) {
                                continue;
                            }
                            if(s + 1 != s_end) {
                                ++s;
                            }
                            if(current_glob_seg + 1 == seg_num) {
                                return true;
                            }
                            ++current_glob_seg;
                            p = p_start + segments[current_glob_seg].start;
                            seg_start = p;
                            seg_end = p_start + segments[current_glob_seg].end;
                        }
                        save_stat();
                    }
                    continue;
                }

                case '?': {
                    if(p + 1 != seg_end && *(p + 1) == '*') {
                        // Handle '?*'
                        unsigned offset = *s == '\\' ? 2 : 1;
                        s += offset;
                        save_stat();
                        p += 2;
                        continue;
                    }
                    if(s != s_end && *s != '/') {
                        ++p;
                        s = *s == '\\' ? s + 2 : s + 1;
                        continue;
                    }
                    break;
                }

                case '[': {
                    if(b < brackets.size() && brackets[b].bytes[uint8_t(*s)]) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        p = pattern.data() + brackets[b].next_offset;
                        ++b;
                        ++s;
                        continue;
                    }
                    break;
                }

                case '\\': {
                    if(p + 1 != seg_end && *(p + 1) == *s) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        p += 2;
                        ++s;
                        continue;
                    }
                    break;
                }

                default: {
                    if(*p == *s) {
                        if(p == seg_start && !(s == s_start || *(s - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        ++p;
                        ++s;
                        continue;
                    }
                    break;
                }
            }

        } else {
            // P comes to a segment end
            if(seg_end != p_end) {
                if(wild_mode) {
                    // Step to next segment
                    ++current_glob_seg;
                    while(s != s_end && *s != '/') {
                        ++s;
                    }
                    if(s != s_end && *s == '/') {
                        ++s;
                    }
                    if(current_glob_seg >= seg_num) {
                        return s == s_end;
                    }
                    p = p_start + segments[current_glob_seg].start;
                    seg_start = p;
                    seg_end = p_start + segments[current_glob_seg].end;
                    continue;
                } else {
                    if(*seg_end != *s) {
                        return false;
                    }
                    // *S and *SegEnd should be '/'
                    // Escape repeat '/'
                    while(s != s_end && *s == '/') {
                        ++s;
                    }
                    // Step to next segment
                    ++current_glob_seg;
                    p = p_start + segments[current_glob_seg].start;
                    seg_start = p;
                    seg_end = p_start + segments[current_glob_seg].end;
                    continue;
                }
            }
        }

        if(backtrace_stack.empty()) {
            return false;
        }

        // Backstrace
        auto& state = backtrace_stack.back();

        p = state.p;
        s = ++state.s;
        b = state.b;
        current_glob_seg = state.glob_seg;
        wild_mode = state.wild_mode;
        seg_start = state.seg_start;
        seg_end = state.seg_end;

        // In non-WildMode, pop back invalid stat in the stack
        if(!wild_mode && (s == s_end || *s == '/')) {
            backtrace_stack.pop_back();
            continue;
        }

        // In non-WildMode, '/' is a barrier
        if(!wild_mode && *s == '/') {
            return false;
        }
    }

    return s == s_end;
}

bool GlobPattern::is_trivial_match_all() const {
    if(!prefix.empty()) [[likely]] {
        return false;
    }

    // a pattern contians only '*' or '/'
    constexpr auto is_wildcard_chain = [](llvm::StringRef pattern) {
        return pattern.drop_while([](char ch) { return ch == '*' || ch == '/'; }).empty();
    };

    if(sub_globs.size() >= 1) {
        return std::ranges::all_of(sub_globs, is_wildcard_chain, &SubPattern::str);
    }

    return false;
}

}  // namespace clice
