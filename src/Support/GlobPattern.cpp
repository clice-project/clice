#include <format>

#include "Support/GlobPattern.h"

namespace clice {

// Expands character ranges and returns a bitmap.
// For example, "a-cf-hz" is expanded to "abcfghz".
static std::expected<llvm::BitVector, std::string> expand(llvm::StringRef s,
                                                          llvm::StringRef original) {
    using namespace llvm;
    BitVector bv{256, false};

    for(size_t i = 0, e = s.size(); i < e; ++i) {
        switch(s[i]) {
            case '\\': {
                if(++i == e) {
                    return std::unexpected{"Invalid expansions: stary `\\`"};
                }
                if(s[i] != '/') {
                    bv[(uint8_t)s[i]] = true;
                }
                break;
            }

            case '-': {
                if(i == 0 || i + 1 == e) {
                    bv['-'] = true;
                    break;
                }
                char c_begin = s[i - 1];
                char c_end = s[i + 1];
                ++i;
                if(c_end == '\\') {
                    if(s[++i] == e) {
                        return std::unexpected{"Invalid expansions: stary `\\`"};
                    }
                    c_end = s[i];
                }
                if(c_begin > c_end) {
                    return std::unexpected{
                        std::format("Invalid expansion: `{}` is larger than `{}`", c_begin, c_end)};
                }
                for(char c = c_begin; c <= c_end; ++c) {
                    if(c != '/') {
                        bv[c] = true;
                    }
                }
                break;
            }

            default: {
                if(s[i] != '/') {
                    bv[(uint8_t)s[i]] = true;
                }
            }
        }
    }

    return bv;
}

// Identify brace expansions in S and return the list of patterns they expand
// into.
static std::expected<llvm::SmallVector<std::string, 1>, std::string>
    parseBraceExpansions(llvm::StringRef s, std::optional<size_t> max_subpattern_num) {
    using namespace llvm;

    SmallVector<std::string> subpatterns = {s.str()};
    if(!max_subpattern_num || !s.contains('{')) {
        return std::move(subpatterns);
    }

    struct BraceExpansion {
        size_t start;
        size_t length;
        SmallVector<StringRef, 2> terms;
    };

    SmallVector<BraceExpansion, 0> brace_expansions;

    BraceExpansion* current_be = nullptr;
    size_t term_begin = 0;
    for(size_t i = 0, e = s.size(); i != e; ++i) {
        if(s[i] == '[') {
            ++i;
            if(s[i] == ']') {
                ++i;
            }
            while(i != e && s[i] != ']') {
                if(s[i++] == '\\') {
                    if(i == e) {
                        return std::unexpected{
                            "Invalid glob pattern, unmatched '[', with stray " "'\\' inside"};
                    }
                    ++i;
                }
            }
            if(i == e) {
                return std::unexpected{"Invalid glob pattern, unmatched '['"};
            }
        } else if(s[i] == '{') {
            if(current_be) {
                return std::unexpected{"Nested brace expansions are not supported"};
            }
            current_be = &brace_expansions.emplace_back();
            current_be->start = i;
            term_begin = i + 1;
        } else if(s[i] == ',') {
            if(!current_be) {
                continue;
            }
            current_be->terms.push_back(s.substr(term_begin, i - term_begin));
            term_begin = i + 1;
        } else if(s[i] == '}') {
            if(!current_be) {
                continue;
            }
            if(current_be->terms.empty() && i - term_begin == 0) {
                return std::unexpected{"Empty brace expression is not supported"};
            }
            current_be->terms.push_back(s.substr(term_begin, i - term_begin));
            current_be->length = i - current_be->start + 1;
            current_be = nullptr;
        } else if(s[i] == '\\') {
            if(++i == e) {
                return std::unexpected{"Invalid glob pattern, stray '\\'"};
            }
        }
    }

    if(current_be) {
        return std::unexpected{"Incomplete brace expansion"};
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
        return std::unexpected{"Too many brace expansions"};
    }

    for(auto& be: reverse(brace_expansions)) {
        SmallVector<std::string> OrigSubPatterns;
        std::swap(subpatterns, OrigSubPatterns);
        for(StringRef Term: be.terms) {
            for(StringRef Orig: OrigSubPatterns) {
                subpatterns.emplace_back(Orig).replace(be.start, be.length, Term);
            }
        }
    }

    return std::move(subpatterns);
}

std::expected<GlobPattern, std::string>
    GlobPattern::create(llvm::StringRef s, std::optional<size_t> max_subpattern_num) {
    using namespace llvm;

    // Store the prefix that does not contain any metacharacter.
    GlobPattern pat;
    size_t prefix_size = s.find_first_of("?*[{\\");
    pat.prefix_at_seg_end = false;
    if(prefix_size == std::string::npos) {
        pat.prefix = s.substr(0, prefix_size);
        return pat;
    }
    if(prefix_size != 0 && s[prefix_size - 1] == '/') {
        pat.prefix_at_seg_end = true;
        --prefix_size;
    }
    pat.prefix = s.substr(0, prefix_size);
    size_t s_size = s.size();
    while(prefix_size < s_size && s[prefix_size] == '/') {
        ++prefix_size;
    }
    s = s.substr(prefix_size);

    SmallVector<std::string, 1> sub_pats;
    if(auto res = parseBraceExpansions(s, max_subpattern_num); res.has_value()) {
        sub_pats = res.value();
    } else {
        return std::unexpected{res.error()};
    }

    for(auto sub_pat: sub_pats) {
        if(auto res = SubGlobPattern::create(sub_pat); res.has_value()) {
            pat.sub_globs.push_back(res.value());
        } else {
            return std::unexpected{res.error()};
        }
    }

    return pat;
}

std::expected<GlobPattern::SubGlobPattern, std::string>
    GlobPattern::SubGlobPattern::create(llvm::StringRef s) {
    using namespace llvm;
    SubGlobPattern pat;
    llvm::SmallVector<GlobSegment, 0> glob_segments;
    GlobSegment* current_gs = &glob_segments.emplace_back();
    current_gs->start = 0;
    pat.pat.assign(s.begin(), s.end());
    // Parse brackets.
    size_t e = s.size();
    for(size_t i = 0; i < e; ++i) {
        if(!current_gs) {
            current_gs = &glob_segments.emplace_back();
            current_gs->start = i;
        }
        if(s[i] == '[') {
            // ']' is allowed as the first character of a character class. '[]'
            // is invalid. So, just skip the first character.
            ++i;
            size_t j = i;
            if(s[j] == ']') {
                ++j;
            }
            while(j != e && s[j] != ']') {
                if(s[j++] == '\\') {
                    if(j == e) {
                        return std::unexpected{
                            "invalid glob pattern, unmatched '[' with stray '\\' inside"};
                    }
                    ++j;
                }
            }
            if(j == e) {
                return std::unexpected{"Invalid glob pattern, unmatched '['"};
            }
            StringRef chars = s.substr(i, j - i);
            bool invert = s[i] == '^' || s[i] == '!';
            auto bv = invert ? expand(chars.substr(1), s) : expand(chars, s);
            if(!bv.has_value()) {
                return std::unexpected{bv.error()};
            }
            if(invert) {
                bv->flip();
            }
            pat.brackets.push_back(Bracket{j + 1, bv.value()});
            i = j;
        } else if(s[i] == '\\') {
            if(++i == e) {
                return std::unexpected{"Invalid glob pattern, stray '\\'"};
            }
        } else if(s[i] == '/') {
            while(i != e && s[i] == '/') {
                ++i;
            }
            if(i == e) {
                break;
            }
            --i;
            current_gs->end = i;
            current_gs = nullptr;
        }
    }

    if(current_gs) {
        current_gs->end = e;
        current_gs = nullptr;
    }

    std::swap(pat.glob_segments, glob_segments);
    return pat;
}

bool GlobPattern::match(llvm::StringRef str) {
    if(!str.consume_front(prefix))
        return false;

    if(str.empty()) {
        if(sub_globs.empty()) {
            return true;
        }
    } else if(prefix_at_seg_end) {
        if(str[0] == '/') {
            str = str.substr(1);
        } else {
            return false;
        }
    }

    for(auto& Glob: sub_globs) {
        if(Glob.match(str)) {
            return true;
        }
    }
    return false;
}

bool GlobPattern::SubGlobPattern::match(llvm::StringRef str) const {
    // P: Current position in the pattern
    // S: Current position in the String
    // B: Current processed bracket num
    const char* s = str.data();
    const char* const s_start = s;
    const char* const s_end = s + str.size();
    const char* p = pat.data();
    const char* seg_start = p;
    const char* const p_start = p;
    const char* const p_end = p + pat.size();
    const char* seg_end = p + glob_segments[0].end;
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
    const size_t seg_num = glob_segments.size();

    while(current_glob_seg < seg_num) {

        if(s == s_end) {
            // Return true if all pattern characters are processed or only
            // '*' or '/' characters remain
            return getPat().find_first_not_of("*/", p - pat.data()) == std::string::npos;
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
                                if(current_glob_seg + 1 != seg_num) {
                                    ++current_glob_seg;
                                    seg_start = p_start + glob_segments[current_glob_seg].start;
                                    seg_end = p_start + glob_segments[current_glob_seg].end;
                                } else {
                                    return true;
                                }
                            }
                            ++p;
                        }
                        if(p == seg_end) {
                            if(current_glob_seg + 1 != seg_num) {
                                // '**' at segment end
                                ++current_glob_seg;
                                while(s != s_end && *s == '/') {
                                    ++s;
                                }
                                p = p_start + glob_segments[current_glob_seg].start;
                                seg_start = p;
                                seg_end = p_start + glob_segments[current_glob_seg].end;
                            } else {
                                return true;
                            }
                        }
                        backtrace_stack.push_back({.b = b,
                                                   .glob_seg = current_glob_seg,
                                                   .wild_mode = wild_mode,
                                                   .p = p,
                                                   .s = s,
                                                   .seg_end = seg_end,
                                                   .seg_start = seg_start});
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
                            p = p_start + glob_segments[current_glob_seg].start;
                            seg_start = p;
                            seg_end = p_start + glob_segments[current_glob_seg].end;
                        }
                        backtrace_stack.push_back({.b = b,
                                                   .glob_seg = current_glob_seg,
                                                   .wild_mode = wild_mode,
                                                   .p = p,
                                                   .s = s,
                                                   .seg_end = seg_end,
                                                   .seg_start = seg_start});
                    }
                    continue;
                }

                case '?': {
                    if(p + 1 != seg_end && *(p + 1) == '*') {
                        // Handle '?*'
                        unsigned Offset = *s == '\\' ? 2 : 1;
                        p += 2;
                        backtrace_stack.push_back({.b = b,
                                                   .glob_seg = current_glob_seg,
                                                   .wild_mode = wild_mode,
                                                   .p = p - 2,
                                                   .s = s + Offset,
                                                   .seg_end = seg_end,
                                                   .seg_start = seg_start});
                        s += Offset;
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
                        p = pat.data() + brackets[b++].next_offset;
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
                    if(current_glob_seg < seg_num) {
                        p = p_start + glob_segments[current_glob_seg].start;
                        seg_start = p;
                        seg_end = p_start + glob_segments[current_glob_seg].end;
                        continue;
                    } else {
                        return s == s_end;
                    }
                } else {
                    if(*seg_end == *s) {
                        // *S and *SegEnd should be '/'
                        // Escape repeat '/'
                        while(s != s_end && *s == '/') {
                            ++s;
                        }
                        // Step to next segment
                        ++current_glob_seg;
                        p = p_start + glob_segments[current_glob_seg].start;
                        seg_start = p;
                        seg_end = p_start + glob_segments[current_glob_seg].end;
                        continue;
                    } else {
                        return false;
                    }
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

        if(!wild_mode && s >= state.seg_end) {
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

}  // namespace clice
