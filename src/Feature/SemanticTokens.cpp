// #include "Compiler/ParsedAST.h"
#include "Feature/SemanticTokens.h"
#include "Compiler/Semantic.h"

namespace clice::feature {

namespace {

struct SemanticToken {
    std::uint32_t line;
    std::uint32_t column;
    std::uint32_t length;
    proto::SemanticTokenType type;
    std::uint32_t modifiers = 0;

    SemanticToken& addModifier(proto::SemanticTokenModifier modifier) {
        modifiers |= 1 << static_cast<std::uint32_t>(modifier);
        return *this;
    }
};

class HighlightBuilder : public SemanticVisitor<HighlightBuilder> {
public:
    HighlightBuilder(ASTInfo& compiler) : SemanticVisitor<HighlightBuilder>(compiler, true) {}

    void handleOccurrence(const clang::Decl* decl, clang::SourceRange range, RelationKind kind) {
        proto::SemanticTokenType type = proto::SemanticTokenType::Invalid;
        if(llvm::isa<clang::NamespaceDecl, clang::NamespaceAliasDecl>(decl)) {
            type = proto::SemanticTokenType::Namespace;
        } else if(llvm::isa<clang::TypedefNameDecl,
                            clang::TemplateTypeParmDecl,
                            clang::TemplateTemplateParmDecl>(decl)) {
            type = proto::SemanticTokenType::Type;
        } else if(llvm::isa<clang::EnumDecl>(decl)) {
            type = proto::SemanticTokenType::Enum;
        } else if(llvm::isa<clang::EnumConstantDecl>(decl)) {
            type = proto::SemanticTokenType::EnumMember;
        } else if(auto TD = llvm::dyn_cast<clang::TagDecl>(decl)) {
            type = TD->isStruct()  ? proto::SemanticTokenType::Struct
                   : TD->isUnion() ? proto::SemanticTokenType::Union
                                   : proto::SemanticTokenType::Class;
        } else if(llvm::isa<clang::FieldDecl>(decl)) {
            type = proto::SemanticTokenType::Field;
        } else if(llvm::isa<clang::ParmVarDecl>(decl)) {
            type = proto::SemanticTokenType::Parameter;
        } else if(llvm::isa<clang::VarDecl, clang::BindingDecl, clang::NonTypeTemplateParmDecl>(
                      decl)) {
            type = proto::SemanticTokenType::Variable;
        } else if(llvm::isa<clang::CXXMethodDecl>(decl)) {
            type = proto::SemanticTokenType::Method;
        } else if(llvm::isa<clang::FunctionDecl>(decl)) {
            type = proto::SemanticTokenType::Function;
        } else if(llvm::isa<clang::LabelDecl>(decl)) {
            type = proto::SemanticTokenType::Label;
        } else if(llvm::isa<clang::ConceptDecl>(decl)) {
            type = proto::SemanticTokenType::Concept;
        } else if(llvm::isa<clang::ImportDecl>(decl)) {
            type = proto::SemanticTokenType::Module;
        }

        if(type != proto::SemanticTokenType::Invalid) {
            addToken(range.getBegin(), type);
        }
    }

    void handleOccurrence(const clang::BuiltinType* type,
                          clang::SourceRange range,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        // llvm::outs() << type->getName(clang::PrintingPolicy({})) << "\n";
        // dump(range.getBegin());
        // dump(range.getEnd());
    }

    void handleOccurrence(const clang::Attr* attr,
                          clang::SourceRange range,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        auto tokens = tokBuf.expandedTokens(range);
        if(auto first = tokens.begin()) {
            auto second = first + 1;
            switch(first->kind()) {
                case clang::tok::identifier: {
                    bool isFirstTokenNamespace = second && second->kind() == clang::tok::coloncolon;
                    if(isFirstTokenNamespace) {
                        addToken(first->location(), proto::SemanticTokenType::Namespace);
                        first += 2;
                    }

                    assert(first && first->kind() == clang::tok::identifier &&
                           "Expecting an identifier token");
                    addToken(first->location(), proto::SemanticTokenType::Attribute);
                    break;
                }
                /// [[using CC: opt(1), debug]]
                case clang::tok::kw_using: {
                    assert(second && second->kind() == clang::tok::identifier);
                    addToken(second->location(), proto::SemanticTokenType::Namespace);
                    break;
                }
                default: {
                    std::terminate();
                }
            }
        }
    }

    void handleOccurrence(clang::SourceLocation keywordLoc,
                          llvm::ArrayRef<clang::syntax::Token> identifiers) {
        addToken(keywordLoc, proto::SemanticTokenType::Keyword);
        for(auto& token: identifiers) {
            if(token.kind() == clang::tok::identifier) {
                addToken(token.location(), proto::SemanticTokenType::Module);
            }
        }
    }

    HighlightBuilder& addToken(clang::SourceLocation location, proto::SemanticTokenType type) {
        auto loc = srcMgr.getPresumedLoc(location);
        tokens.emplace_back(SemanticToken{
            .line = loc.getLine() - 1,
            .column = loc.getColumn() - 1,
            .length = clang::Lexer::MeasureTokenLength(location, srcMgr, sema.getLangOpts()),
            .type = type,
            .modifiers = 0,
        });
        return *this;
    }

    proto::SemanticTokens build() {
        /// Collect semantic from spelled tokens.
        auto mainFileID = srcMgr.getMainFileID();
        auto spelledTokens = tokBuf.spelledTokens(mainFileID);
        for(auto& token: spelledTokens) {
            proto::SemanticTokenType type = proto::SemanticTokenType::Invalid;
            // llvm::outs() << clang::tok::getTokenName(token.kind()) << " "
            //              << pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())
            //              << "\n";

            auto kind = token.kind();
            switch(kind) {
                case clang::tok::numeric_constant: {
                    type = proto::SemanticTokenType::Number;
                    break;
                }

                case clang::tok::char_constant:
                case clang::tok::wide_char_constant:
                case clang::tok::utf8_char_constant:
                case clang::tok::utf16_char_constant:
                case clang::tok::utf32_char_constant: {
                    type = proto::SemanticTokenType::Character;
                    break;
                }

                case clang::tok::string_literal:
                case clang::tok::wide_string_literal:
                case clang::tok::utf8_string_literal:
                case clang::tok::utf16_string_literal:
                case clang::tok::utf32_string_literal: {
                    type = proto::SemanticTokenType::String;
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
                        type = proto::SemanticTokenType::Operator;
                    }
                }

                default: {
                    if(pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())) {
                        type = proto::SemanticTokenType::Keyword;
                        break;
                    }
                }
            }

            if(type != proto::SemanticTokenType::Invalid) {
                addToken(token.location(), type);
            }
        }

        /// Collect semantic tokens from AST.
        run();

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
            result.data.push_back(llvm::to_underlying(token.type));
            result.data.push_back(token.modifiers);

            lastLine = token.line;
            lastColumn = token.column;
        }

        return result;
    }

    std::vector<SemanticToken> tokens;
};

}  // namespace

proto::SemanticTokens semanticTokens(ASTInfo& info, llvm::StringRef filename) {
    HighlightBuilder builder(info);
    return builder.build();
}

}  // namespace clice::feature
