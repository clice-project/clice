#include "AST/Semantic.h"
#include "Index/Shared.h"
#include "Support/Ranges.h"
#include "Support/Compare.h"
#include "Feature/SemanticTokens.h"

namespace clice::feature {

namespace {

class SemanticTokensCollector : public SemanticVisitor<SemanticTokensCollector> {
public:
    SemanticTokensCollector(ASTInfo& AST, bool interestedOnly) :
        emitForIndex(!interestedOnly),
        SemanticVisitor<SemanticTokensCollector>(AST, interestedOnly) {}

    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        SymbolModifiers modifiers;

        if(kind == RelationKind::Definition) {
            modifiers |= SymbolModifiers::Definition;
        } else if(kind == RelationKind::Declaration) {
            modifiers |= SymbolModifiers::Declaration;
        }

        if(isTemplated(decl)) {
            modifiers |= SymbolModifiers::Templated;
        }

        /// TODO: Add more modifiers.

        addToken(location, SymbolKind::from(decl), modifiers);
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        SymbolModifiers modifiers;

        if(kind == RelationKind::Definition) {
            modifiers |= SymbolModifiers::Definition;
        } else if(kind == RelationKind::Declaration) {
            modifiers |= SymbolModifiers::Declaration;
        }

        addToken(location, SymbolKind::Macro, modifiers);
    }

    /// FIXME: Handle module name occurrence.

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

    auto buildForFile() {
        highlight(AST.getInterestedFile());
        run();
        merge(result);
        return std::move(result);
    }

    auto buildForIndex() {
        for(auto fid: AST.files()) {
            highlight(fid);
        }

        run();

        for(auto& [fid, tokens]: sharedResult) {
            merge(tokens);
        }

        return std::move(sharedResult);
    }

private:
    void addToken(clang::FileID fid, const clang::Token& token, SymbolKind kind) {
        auto fake = clang::SourceLocation::getFromRawEncoding(1);
        auto offset = token.getLocation().getRawEncoding() - fake.getRawEncoding();
        LocalSourceRange range{offset, offset + token.getLength()};

        auto& tokens = emitForIndex ? sharedResult[fid] : result;
        tokens.emplace_back(range, kind, SymbolModifiers());
    }

    void addToken(clang::SourceLocation location, SymbolKind kind, SymbolModifiers modifiers) {
        if(location.isMacroID()) {
            auto spelling = AST.getSpellingLoc(location);
            auto expansion = AST.getExpansionLoc(location);

            /// FIXME: For location from macro, we only handle the case that the
            /// spelling and expansion are in the same file currently.
            if(AST.getFileID(spelling) != AST.getFileID(expansion)) {
                return;
            }

            /// For occurrence, we always use spelling location.
            location = spelling;
        }

        auto [fid, range] = AST.toLocalRange(location);
        auto& tokens = emitForIndex ? sharedResult[fid] : result;
        tokens.emplace_back(range, kind, modifiers);
    }

    /// Render semantic tokens for file through raw lexer.
    void highlight(clang::FileID fid) {
        auto content = getFileContent(SM, fid);
        auto& langOpts = PP.getLangOpts();

        /// Whether the token is after `#`.
        bool isAfterHash = false;
        /// Whether the token is in the header name.
        bool isInHeader = false;
        /// Whether the token is in the directive line.
        bool isInDirectiveLine = false;

        /// Use to distinguish whether the token is in a keyword.
        clang::IdentifierTable identifierTable(PP.getLangOpts());

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

    void resolve(SemanticToken& last, const SemanticToken& current) {
        /// FIXME: Add more rules to resolve kind conflict.
        if(last.kind == SymbolKind::Conflict) {
            return;
        }

        last.kind = SymbolKind::Conflict;
    }

    /// Merge tokens with same range and resolve kind conflict.
    void merge(std::vector<SemanticToken>& tokens) {
        /// Sort tokens by range.
        std::ranges::sort(tokens, refl::less, [](const auto& token) { return token.range; });

        std::vector<SemanticToken> merged;

        for(auto& token: tokens) {
            if(merged.empty()) {
                merged.emplace_back(token);
                continue;
            }

            auto& last = merged.back();
            if(last.range == token.range) {
                /// If the token has same range, we need to resolve the kind conflict.
                resolve(last, token);
            } else if(last.range.end == token.range.begin && last.kind == token.kind) {
                /// If the token has same kind and adjacent range, we need to merge them.
                last.range.end = token.range.end;
            } else {
                /// Otherwise, we just append the token.
                merged.emplace_back(token);
            }
        }

        tokens = std::move(merged);
    }

private:
    bool emitForIndex;
    std::vector<SemanticToken> result;
    index::Shared<std::vector<SemanticToken>> sharedResult;
};

}  // namespace

std::vector<SemanticToken> semanticTokens(ASTInfo& AST) {
    return SemanticTokensCollector(AST, true).buildForFile();
}

index::Shared<std::vector<SemanticToken>> indexSemanticTokens(ASTInfo& AST) {
    return SemanticTokensCollector(AST, false).buildForIndex();
}

}  // namespace clice::feature
