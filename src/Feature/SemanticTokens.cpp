#include "AST/Semantic.h"
#include "Index/Shared.h"
#include "Feature/SemanticTokens.h"

namespace clice::feature {

namespace {

class HighlightBuilder : public SemanticVisitor<HighlightBuilder> {
public:
    HighlightBuilder(ASTInfo& info, bool emitForIndex) :
        emitForIndex(emitForIndex), SemanticVisitor<HighlightBuilder>(info, true) {}

    void addToken(clang::FileID fid, const clang::Token& token, SymbolKind kind) {
        auto fake = clang::SourceLocation::getFromRawEncoding(1);
        LocalSourceRange range = {
            token.getLocation().getRawEncoding() - fake.getRawEncoding(),
            token.getEndLoc().getRawEncoding() - fake.getRawEncoding(),
        };

        auto& tokens = emitForIndex ? sharedResult[fid] : result;
        tokens.emplace_back(SemanticToken{
            .range = range,
            .kind = kind,
            .modifiers = {},
        });
    }

    void addToken(clang::SourceLocation location, SymbolKind kind, SymbolModifiers modifiers) {
        auto& SM = srcMgr;
        /// Always use spelling location.
        auto spelling = SM.getSpellingLoc(location);
        auto [fid, offset] = SM.getDecomposedLoc(spelling);

        /// If the spelling location is not in the interested file and not for index, skip it.
        if(fid != SM.getMainFileID() && !emitForIndex) {
            return;
        }

        auto& tokens = emitForIndex ? sharedResult[fid] : result;
        auto length = getTokenLength(SM, spelling);
        tokens.emplace_back(SemanticToken{
            .range = {offset, offset + length},
            .kind = kind,
            .modifiers = modifiers,
        });
    }

    /// Render semantic tokens from lexer. Note that we only render literal,
    /// directive, keyword, and comment tokens.
    void highlightFromLexer(clang::FileID fid) {
        auto& SM = srcMgr;
        auto content = getFileContent(SM, fid);
        auto& langOpts = pp.getLangOpts();

        /// Whether the token is after `#`.
        bool isAfterHash = false;
        /// Whether the token is in the header name.
        bool isInHeader = false;
        /// Whether the token is in the directive line.
        bool isInDirectiveLine = false;

        /// Use to distinguish whether the token is in a keyword.
        clang::IdentifierTable identifierTable(pp.getLangOpts());

        auto callback = [&](const clang::Token& token) -> bool {
            SymbolKind kind = SymbolKind::Invalid;

            /// Clear the all states.
            if(token.isAtStartOfLine()) {
                isInHeader = false;
                isInDirectiveLine = false;
            }

            if(isInHeader) {
                addToken(fid, token, SymbolKind::Header);
                return true;
            }

            switch(token.getKind()) {
                case clang::tok::comment: {
                    kind = SymbolKind::Comment;
                    break;
                }

                case clang::tok::numeric_constant: {
                    kind = SymbolKind::Number;
                    break;
                }

                case clang::tok::char_constant:
                case clang::tok::wide_char_constant:
                case clang::tok::utf8_char_constant:
                case clang::tok::utf16_char_constant:
                case clang::tok::utf32_char_constant: {
                    kind = SymbolKind::Character;
                    break;
                }

                case clang::tok::string_literal: {
                    kind = SymbolKind::String;
                    break;
                }

                case clang::tok::wide_string_literal:
                case clang::tok::utf8_string_literal:
                case clang::tok::utf16_string_literal:
                case clang::tok::utf32_string_literal: {
                    kind = SymbolKind::String;
                    break;
                }

                case clang::tok::hash: {
                    if(token.isAtStartOfLine()) {
                        isAfterHash = true;
                        isInDirectiveLine = true;
                        kind = SymbolKind::Directive;
                    }
                    break;
                }

                case clang::tok::raw_identifier: {
                    auto spelling = token.getRawIdentifier();
                    if(isAfterHash) {
                        isAfterHash = false;
                        isInHeader = (spelling == "include");
                        kind = SymbolKind::Directive;
                    } else if(isInHeader) {
                        kind = SymbolKind::Header;
                    } else if(isInDirectiveLine) {
                        if(spelling == "defined") {
                            kind = SymbolKind::Directive;
                        }
                    } else {
                        /// Check whether the identifier is a keyword.
                        if(auto& II = identifierTable.get(spelling); II.isKeyword(langOpts)) {
                            kind = SymbolKind::Keyword;
                        }
                    }
                }

                default: {
                    break;
                }
            }

            if(kind != SymbolKind::Invalid) {
                addToken(fid, token, kind);
            }

            return true;
        };

        tokenize(content, callback, false, &langOpts);
    }

    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        /// FIXME: Add modifiers.
        addToken(location, SymbolKind::from(decl), {});
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// FIXME: Add modifiers.
        addToken(location, SymbolKind::Macro, {});
    }

    void handleAttrOccurrence(const clang::Attr* attr, clang::SourceRange range) {
        /// Render `final` and `override` attributes. We cannot determine only by
        /// lexer, so we need to render them here.
        auto [begin, end] = range;
        if(auto FA = clang::dyn_cast<clang::FinalAttr>(attr)) {
            assert(begin == end && "Invalid range");
            addToken(begin, SymbolKind::Keyword, {});
        } else if(auto OA = clang::dyn_cast<clang::OverrideAttr>(attr)) {
            assert(begin == end && "Invalid range");
            addToken(begin, SymbolKind::Keyword, {});
        }
    }

    /// FIXME: handle module name.

    void merge(std::vector<SemanticToken>& tokens) {
        ranges::sort(tokens, refl::less, [](const auto& token) { return token.range; });

        std::vector<SemanticToken> merged;

        std::size_t i = 0;
        while(i < tokens.size()) {
            LocalSourceRange range = tokens[i].range;
            auto begin = i;

            /// Find all tokens with same range.
            while(i < tokens.size() && refl::equal(tokens[i].range, range)) {
                i++;
            }

            auto end = i;

            /// Merge all tokens with same range.
            /// FIXME: Determine SymbolKind properly.

            SymbolKind kind = tokens[begin].kind;
            SymbolModifiers modifiers = tokens[begin].modifiers;

            if(!merged.empty()) {
                auto& last = merged.back();
                if(last.kind == kind && last.range.end == range.begin) {
                    last.range.end = range.end;
                    continue;
                }
            }

            merged.emplace_back(SemanticToken{
                .range = range,
                .kind = kind,
                .modifiers = modifiers,
            });
        }

        tokens = std::move(merged);
    }

    auto buildForFile() {
        highlightFromLexer(info.getInterestedFile());
        run();
        merge(result);
        return std::move(result);
    }

    auto buildForIndex() {
        for(auto fid: info.files()) {
            highlightFromLexer(fid);
        }

        run();

        for(auto& [fid, tokens]: sharedResult) {
            merge(tokens);
        }

        return std::move(sharedResult);
    }

private:
    std::vector<SemanticToken> result;
    index::Shared<std::vector<SemanticToken>> sharedResult;
    bool emitForIndex;
};

}  // namespace

index::Shared<std::vector<SemanticToken>> semanticTokens(ASTInfo& info) {
    return HighlightBuilder(info, true).buildForIndex();
}

proto::SemanticTokens toSemanticTokens(llvm::ArrayRef<SemanticToken> tokens,
                                       SourceConverter& SC,
                                       llvm::StringRef content,
                                       const config::SemanticTokensOption& option) {

    proto::SemanticTokens result;

    std::size_t lastLine = 0;
    std::size_t lastColumn = 0;

    for(auto& token: tokens) {
        auto [begin, end] = token.range;
        auto [line, column] = SC.toPosition(content, begin);

        if(line != lastLine) {
            /// FIXME: Cut off content to improve performance.
            lastColumn = 0;
        }

        result.data.emplace_back(line - lastLine);
        result.data.emplace_back(column - lastColumn);
        result.data.emplace_back(end - begin);
        result.data.emplace_back(token.kind.value());
        result.data.emplace_back(token.modifiers.value());

        lastLine = line;
        lastColumn = column;
    }

    return result;
}

proto::SemanticTokens semanticTokens(ASTInfo& info,
                                     SourceConverter& SC,
                                     const config::SemanticTokensOption& option) {
    return {};
}

}  // namespace clice::feature
