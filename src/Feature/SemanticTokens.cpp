// #include "Compiler/ParsedAST.h"
#include "Feature/SemanticTokens.h"
#include "Compiler/Semantic.h"

namespace clice::feature {

namespace {

class HighlightBuilder : public SemanticVisitor<HighlightBuilder> {
public:
    HighlightBuilder(ASTInfo& compiler) : SemanticVisitor<HighlightBuilder>(compiler, true) {}

    void run() {
        TraverseAST(sema.getASTContext());
    }

    proto::SemanticTokens build() {
        /// Collect semantic from spelled tokens.
        auto mainFileID = srcMgr.getMainFileID();
        auto spelledTokens = tokBuf.spelledTokens(mainFileID);
        for(auto& token: spelledTokens) {
            SymbolKind type = SymbolKind::Invalid;
            // llvm::outs() << clang::tok::getTokenName(token.kind()) << " "
            //              << pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())
            //              << "\n";

            auto kind = token.kind();
            switch(kind) {
                case clang::tok::numeric_constant: {
                    type = SymbolKind::Number;
                    break;
                }

                case clang::tok::char_constant:
                case clang::tok::wide_char_constant:
                case clang::tok::utf8_char_constant:
                case clang::tok::utf16_char_constant:
                case clang::tok::utf32_char_constant: {
                    type = SymbolKind::Character;
                    break;
                }

                case clang::tok::string_literal:
                case clang::tok::wide_string_literal:
                case clang::tok::utf8_string_literal:
                case clang::tok::utf16_string_literal:
                case clang::tok::utf32_string_literal: {
                    type = SymbolKind::String;
                    break;
                }

                case clang::tok::ampamp:        /// and
                case clang::tok::ampequal:      /// and_eq
                case clang::tok::amp:           /// bitand
                case clang::tok::pipe:          /// bitor
                case clang::tok::tilde:         /// compl
                case clang::tok::exclaim:       /// not
                case clang::tok::exclaimequal:  /// not_eq
                case clang::tok::pipepipe:      /// or
                case clang::tok::pipeequal:     /// or_eq
                case clang::tok::caret:         /// xor
                case clang::tok::caretequal: {  /// xor_eq
                    /// Clang will lex above keywords as corresponding operators. But we want to
                    /// highlight them as keywords. So check whether their text is same as the
                    /// operator spelling. If not, it indicates that they are keywords.
                    if(token.text(srcMgr) != clang::tok::getPunctuatorSpelling(kind)) {
                        type = SymbolKind::Operator;
                    }
                }

                default: {
                    if(pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())) {
                        type = SymbolKind::Keyword;
                        break;
                    }
                }
            }

            if(type != SymbolKind::Invalid) {}
        }

        /// Collect semantic tokens from AST.
        TraverseAST(sema.getASTContext());

        proto::SemanticTokens result;
        std::ranges::sort(tokens, [](const SemanticToken& lhs, const SemanticToken& rhs) {
            return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
        });

        std::size_t lastLine = 0;
        std::size_t lastColumn = 0;

        for(auto& token: tokens) {
            result.data.push_back(token.line - lastLine);
            result.data.push_back(token.line == lastLine ? token.column - lastColumn
                                                         : token.column);
            result.data.push_back(token.length);
            // result.data.push_back(llvm::to_underlying(token.column));
            // result.data.push_back(0);

            lastLine = token.line;
            lastColumn = token.column;
        }

        return result;
    }

    std::vector<SemanticToken> tokens;
};

}  // namespace

index::SharedIndex<std::vector<SemanticToken>> semanticTokens(ASTInfo& info) {
    return index::SharedIndex<std::vector<SemanticToken>>{};
}

proto::SemanticTokens toSemanticTokens(llvm::ArrayRef<SemanticToken> tokens,
                                       const config::SemanticTokensOption& option) {
    return {};
}

proto::SemanticTokens semanticTokens(ASTInfo& info, const config::SemanticTokensOption& option) {
    return {};
}

}  // namespace clice::feature
