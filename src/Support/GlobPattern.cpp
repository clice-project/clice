#include <format>

#include "Support/GlobPattern.h"

namespace clice {

// Expands character ranges and returns a bitmap.
// For example, "a-cf-hz" is expanded to "abcfghz".
static std::expected<llvm::BitVector, std::string> expand(llvm::StringRef S,
                                                          llvm::StringRef Original) {
    using namespace llvm;
    BitVector BV(256, false);

    for(size_t I = 0, E = S.size(); I < E; ++I) {
        switch(S[I]) {
            case '\\':
                if(++I == E) {
                    return std::unexpected{"Invalid expansions: stary `\\`"};
                }
                if(S[I] != '/') {
                    BV[(uint8_t)S[I]] = true;
                }
                break;
            case '-': {
                if(I == 0 || I + 1 == E) {
                    BV['-'] = true;
                    break;
                }
                char CBegin = S[I - 1];
                char CEnd = S[I + 1];
                ++I;
                if(CEnd == '\\') {
                    if(S[++I] == E) {
                        return std::unexpected{"Invalid expansions: stary `\\`"};
                    }
                    CEnd = S[I];
                }
                if(CBegin > CEnd) {
                    return std::unexpected{
                        std::format("Invalid expansion: `{}` is larger than `{}`", CBegin, CEnd)};
                }
                for(char C = CBegin; C <= CEnd; ++C) {
                    if(C != '/') {
                        BV[C] = true;
                    }
                }
                break;
            }
            default:
                if(S[I] != '/') {
                    BV[(uint8_t)S[I]] = true;
                }
        }
    }

    return BV;
}

// Identify brace expansions in S and return the list of patterns they expand
// into.
static std::expected<llvm::SmallVector<std::string, 1>, std::string>
    parseBraceExpansions(llvm::StringRef S, std::optional<size_t> MaxSubPatterns) {
    using namespace llvm;

    SmallVector<std::string> SubPatterns = {S.str()};
    if(!MaxSubPatterns || !S.contains('{')) {
        return std::move(SubPatterns);
    }

    struct BraceExpansion {
        size_t Start;
        size_t Length;
        SmallVector<StringRef, 2> Terms;
    };

    SmallVector<BraceExpansion, 0> BraceExpansions;

    BraceExpansion* CurrentBE = nullptr;
    size_t TermBegin = 0;
    for(size_t I = 0, E = S.size(); I != E; ++I) {
        if(S[I] == '[') {
            ++I;
            if(S[I] == ']') {
                ++I;
            }
            while(I != E && S[I] != ']') {
                if(S[I++] == '\\') {
                    if(I == E) {
                        return std::unexpected{
                            "Invalid glob pattern, unmatched '[', with stray " "'\\' inside"};
                    }
                    ++I;
                }
            }
            if(I == E) {
                return std::unexpected{"Invalid glob pattern, unmatched '['"};
            }
        } else if(S[I] == '{') {
            if(CurrentBE) {
                return std::unexpected{"Nested brace expansions are not supported"};
            }
            CurrentBE = &BraceExpansions.emplace_back();
            CurrentBE->Start = I;
            TermBegin = I + 1;
        } else if(S[I] == ',') {
            if(!CurrentBE) {
                continue;
            }
            CurrentBE->Terms.push_back(S.substr(TermBegin, I - TermBegin));
            TermBegin = I + 1;
        } else if(S[I] == '}') {
            if(!CurrentBE) {
                continue;
            }
            if(CurrentBE->Terms.empty() && I - TermBegin == 0) {
                return std::unexpected{"Empty brace expression is not supported"};
            }
            CurrentBE->Terms.push_back(S.substr(TermBegin, I - TermBegin));
            CurrentBE->Length = I - CurrentBE->Start + 1;
            CurrentBE = nullptr;
        } else if(S[I] == '\\') {
            if(++I == E) {
                return std::unexpected{"Invalid glob pattern, stray '\\'"};
            }
        }
    }

    if(CurrentBE) {
        return std::unexpected{"Incomplete brace expansion"};
    }

    size_t NumSubPatterns = 1;
    for(auto& BE: BraceExpansions) {
        if(NumSubPatterns > std::numeric_limits<size_t>::max() / BE.Terms.size()) {
            NumSubPatterns = std::numeric_limits<size_t>::max();
            break;
        }
        NumSubPatterns *= BE.Terms.size();
    }

    if(NumSubPatterns > *MaxSubPatterns) {
        return std::unexpected{"Too many brace expansions"};
    }

    for(auto& BE: reverse(BraceExpansions)) {
        SmallVector<std::string> OrigSubPatterns;
        std::swap(SubPatterns, OrigSubPatterns);
        for(StringRef Term: BE.Terms) {
            for(StringRef Orig: OrigSubPatterns) {
                SubPatterns.emplace_back(Orig).replace(BE.Start, BE.Length, Term);
            }
        }
    }

    return std::move(SubPatterns);
}

std::expected<GlobPattern, std::string> GlobPattern::create(llvm::StringRef S,
                                                            std::optional<size_t> MaxSubPatterns) {
    using namespace llvm;

    // Store the prefix that does not contain any metacharacter.
    GlobPattern Pat;
    size_t PrefixSize = S.find_first_of("?*[{\\");
    Pat.PrefixAtSegEnd = false;
    if(PrefixSize == std::string::npos) {
        Pat.Prefix = S.substr(0, PrefixSize);
        return Pat;
    }
    if(PrefixSize != 0 && S[PrefixSize - 1] == '/') {
        Pat.PrefixAtSegEnd = true;
        --PrefixSize;
    }
    Pat.Prefix = S.substr(0, PrefixSize);
    size_t SSize = S.size();
    while(PrefixSize < SSize && S[PrefixSize] == '/') {
        ++PrefixSize;
    }
    S = S.substr(PrefixSize);

    SmallVector<std::string, 1> SubPats;
    if(auto R = parseBraceExpansions(S, MaxSubPatterns); R.has_value()) {
        SubPats = R.value();
    } else {
        return std::unexpected{R.error()};
    }

    for(auto SubPat: SubPats) {
        if(auto R = SubGlobPattern::create(SubPat); R.has_value()) {
            Pat.SubGlobs.push_back(R.value());
        } else {
            return std::unexpected{R.error()};
        }
    }

    return Pat;
}

std::expected<GlobPattern::SubGlobPattern, std::string>
    GlobPattern::SubGlobPattern::create(llvm::StringRef S) {
    using namespace llvm;
    SubGlobPattern Pat;
    llvm::SmallVector<GlobSegement, 0> GlobSegements;
    GlobSegement* CurrentGS = &GlobSegements.emplace_back();
    CurrentGS->Start = 0;
    Pat.Pat.assign(S.begin(), S.end());
    // Parse brackets.
    size_t E = S.size();
    for(size_t I = 0; I < E; ++I) {
        if(!CurrentGS) {
            CurrentGS = &GlobSegements.emplace_back();
            CurrentGS->Start = I;
        }
        if(S[I] == '[') {
            // ']' is allowed as the first character of a character class. '[]'
            // is invalid. So, just skip the first character.
            ++I;
            size_t J = I;
            if(S[J] == ']') {
                ++J;
            }
            while(J != E && S[J] != ']') {
                if(S[J++] == '\\') {
                    if(J == E) {
                        return std::unexpected{
                            "invalid glob pattern, unmatched '[' with stray '\\' inside"};
                    }
                    ++J;
                }
            }
            if(J == E) {
                return std::unexpected{"Invalid glob pattern, unmatched '['"};
            }
            StringRef Chars = S.substr(I, J - I);
            bool Invert = S[I] == '^' || S[I] == '!';
            auto BV = Invert ? expand(Chars.substr(1), S) : expand(Chars, S);
            if(!BV.has_value()) {
                return std::unexpected{BV.error()};
            }
            if(Invert) {
                BV->flip();
            }
            Pat.Brackets.push_back(Bracket{J + 1, BV.value()});
            I = J;
        } else if(S[I] == '\\') {
            if(++I == E) {
                return std::unexpected{"Invalid glob pattern, stray '\\'"};
            }
        } else if(S[I] == '/') {
            while(I != E && S[I] == '/') {
                ++I;
            }
            if(I == E) {
                break;
            }
            --I;
            CurrentGS->End = I;
            CurrentGS = nullptr;
        }
    }

    if(CurrentGS) {
        CurrentGS->End = E;
        CurrentGS = nullptr;
    }

    std::swap(Pat.GlobSegements, GlobSegements);
    return Pat;
}

bool GlobPattern::match(llvm::StringRef S) {
    if(!S.consume_front(Prefix))
        return false;

    if(S.empty()) {
        if(SubGlobs.empty()) {
            return true;
        }
    } else {
        if(PrefixAtSegEnd) {
            if(S[0] == '/') {
                S = S.substr(1);
            } else {
                return false;
            }
        }
    }

    for(auto& Glob: SubGlobs) {
        if(Glob.match(S)) {
            return true;
        }
    }
    return false;
}

bool GlobPattern::SubGlobPattern::match(llvm::StringRef Str) const {
    // P: Current position in the pattern
    // S: Current position in the String
    // B: Current processed bracket num
    const char *S = Str.data(), *P = nullptr, *SegStart = nullptr, *SegEnd = nullptr;

    // Initialize
    P = SegStart = Pat.data();
    SegEnd = P + GlobSegements[0].End;

    const char *const SEnd = S + Str.size(), *const SStart = S, *const PStart = P,
                      *const PEnd = P + Pat.size();

    size_t B = 0, CurrentGlobSeg = 0;
    bool WildMode = false;

    struct BacktraceStat {
        size_t B, GlobSeg;
        bool WildMode;
        const char *P, *S, *SegEnd, *SegStart;
    };

    llvm::SmallVector<BacktraceStat, 6> BacktraceStack;
    const size_t SegNum = GlobSegements.size();

    while(CurrentGlobSeg < SegNum) {

        if(S == SEnd) {
            // Return true if all pattern characters are processed or only
            // '*' or '/' characters remain
            return getPat().find_first_not_of("*/", P - Pat.data()) == std::string::npos;
        }

        if(P != SegEnd) {
            switch(*P) {
                case '*': {
                    if(P + 1 != PEnd && *(P + 1) == '*') {
                        // Met '**'
                        P += 2;
                        WildMode = true;
                        // Consume extra '*'
                        while(P != PEnd && (*P == '*' || *P == '/')) {
                            if(*P == '/') {
                                if(CurrentGlobSeg + 1 != SegNum) {
                                    ++CurrentGlobSeg;
                                    SegStart = PStart + GlobSegements[CurrentGlobSeg].Start;
                                    SegEnd = PStart + GlobSegements[CurrentGlobSeg].End;
                                } else {
                                    return true;
                                }
                            }
                            ++P;
                        }

                        if(P == SegEnd) {
                            if(CurrentGlobSeg + 1 != SegNum) {
                                // '**' at segement end
                                ++CurrentGlobSeg;
                                while(S != SEnd && *S == '/') {
                                    ++S;
                                }
                                P = PStart + GlobSegements[CurrentGlobSeg].Start;
                                SegStart = P;
                                SegEnd = PStart + GlobSegements[CurrentGlobSeg].End;
                            } else {
                                return true;
                            }
                        }

                        BacktraceStack.push_back({.B = B,
                                                  .GlobSeg = CurrentGlobSeg,
                                                  .WildMode = WildMode,
                                                  .P = P,
                                                  .S = S,
                                                  .SegEnd = SegEnd,
                                                  .SegStart = SegStart});
                    } else {
                        // Met '*'
                        ++P;
                        WildMode = false;
                        if(P == SegEnd) {
                            while(S != SEnd && *S != '/') {
                                ++S;
                            }
                            if(S == SEnd) {
                                continue;
                            }
                            if(S + 1 != SEnd) {
                                ++S;
                            }
                            if(CurrentGlobSeg + 1 == SegNum) {
                                return true;
                            }
                            ++CurrentGlobSeg;
                            P = PStart + GlobSegements[CurrentGlobSeg].Start;
                            SegStart = P;
                            SegEnd = PStart + GlobSegements[CurrentGlobSeg].End;
                        }
                        BacktraceStack.push_back({.B = B,
                                                  .GlobSeg = CurrentGlobSeg,
                                                  .WildMode = WildMode,
                                                  .P = P,
                                                  .S = S,
                                                  .SegEnd = SegEnd,
                                                  .SegStart = SegStart});
                    }
                    continue;
                }

                case '?':
                    if(P + 1 != SegEnd && *(P + 1) == '*') {
                        // Handle '?*'
                        unsigned Offset = *S == '\\' ? 2 : 1;
                        P += 2;
                        BacktraceStack.push_back({.B = B,
                                                  .GlobSeg = CurrentGlobSeg,
                                                  .WildMode = WildMode,
                                                  .P = P - 2,
                                                  .S = S + Offset,
                                                  .SegEnd = SegEnd,
                                                  .SegStart = SegStart});
                        S += Offset;
                        continue;
                    }
                    if(S != SEnd && *S != '/') {
                        ++P;
                        S = *S == '\\' ? S + 2 : S + 1;
                        continue;
                    }
                    break;

                case '[':
                    if(B < Brackets.size() && Brackets[B].Bytes[uint8_t(*S)]) {
                        if(P == SegStart && !(S == SStart || *(S - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        P = Pat.data() + Brackets[B++].NextOffset;
                        ++S;
                        continue;
                    }
                    break;

                case '\\':
                    if(P + 1 != SegEnd && *(P + 1) == *S) {
                        if(P == SegStart && !(S == SStart || *(S - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        P += 2;
                        ++S;
                        continue;
                    }
                    break;

                default:
                    if(*P == *S) {
                        if(P == SegStart && !(S == SStart || *(S - 1) == '/')) {
                            // Segment start dismatch
                            break;
                        }
                        ++P;
                        ++S;
                        continue;
                    }
                    break;
            }
        } else {
            // P comes to a segement end
            if(SegEnd != PEnd) {
                if(WildMode) {
                    // Step to next segement
                    ++CurrentGlobSeg;
                    while(S != SEnd && *S != '/') {
                        ++S;
                    }
                    if(S != SEnd && *S == '/') {
                        ++S;
                    }
                    if(CurrentGlobSeg < SegNum) {
                        P = PStart + GlobSegements[CurrentGlobSeg].Start;
                        SegStart = P;
                        SegEnd = PStart + GlobSegements[CurrentGlobSeg].End;
                        continue;
                    } else {
                        return S == SEnd;
                    }
                } else {
                    if(*SegEnd == *S) {
                        // *S and *SegEnd should be '/'
                        // Escape repeat '/'
                        while(S != SEnd && *S == '/') {
                            ++S;
                        }

                        // Step to next segement
                        ++CurrentGlobSeg;
                        P = PStart + GlobSegements[CurrentGlobSeg].Start;
                        SegStart = P;
                        SegEnd = PStart + GlobSegements[CurrentGlobSeg].End;
                        continue;
                    } else {
                        return false;
                    }
                }
            }
        }

        if(BacktraceStack.empty()) {
            return false;
        }

        // Backstrace
        auto& State = BacktraceStack.back();

        P = State.P;
        S = ++State.S;
        B = State.B;
        CurrentGlobSeg = State.GlobSeg;
        WildMode = State.WildMode;
        SegStart = State.SegStart;
        SegEnd = State.SegEnd;

        if(!WildMode && S >= State.SegEnd) {
            BacktraceStack.pop_back();
            continue;
        }

        // In non-WildMode, '/' is a barrier
        if(!WildMode && *S == '/') {
            return false;
        }
    }

    return S == SEnd;
}

}  // namespace clice
