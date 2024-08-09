#include <Clang/ParsedAST.h>
#include <Feature/SemanticToken.h>

namespace clice::feature {

#define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
#define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)
#define VISIT_TYPE(NAME) bool Visit##NAME(clang::NAME node)

using protocol::SemanticTokenType;
using protocol::SemanticTokenModifier;

class SemanticToken {
private:
    clang::SourceRange range;
    SemanticTokenType type;
    uint32_t modifiers;

private:

public:
    SemanticToken(SemanticTokenType type, clang::SourceLocation begin, clang::SourceLocation end) :
        range(begin, end), type(type), modifiers(0) {}

    SemanticToken(SemanticTokenType type, const clang::syntax::Token& token) :
        range(token.location(), token.endLocation()), type(type), modifiers(0) {}

    SemanticToken& addModifier(SemanticTokenModifier modifier) {
        modifiers |= (static_cast<std::underlying_type_t<SemanticTokenModifier>>(modifier) << 1);
        return *this;
    }

    // SemanticToken& add() { int x = 1; }
};

class HighlightCollector : public clang::RecursiveASTVisitor<HighlightCollector> {
private:
    clang::ASTContext& context;
    clang::Preprocessor& preprocessor;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer& tokenBuffer;
    std::vector<SemanticToken> tokens;

public:
    HighlightCollector(ParsedAST& AST) :
        context(AST.ASTContext()), preprocessor(AST.Preprocessor()),
        sourceManager(AST.SourceManager()), tokenBuffer(AST.TokenBuffer()) {}

    std::vector<SemanticToken> collect() && {

        // highlight tokens directly from token buffer.
        // mainly about directives, macros, keywords, comments, operators, and literals.
        // TraverseTokens();

        // highlight tokens from AST.
        // mainly about types, functions, variables and namespaces.
        TraverseTranslationUnitDecl(context.getTranslationUnitDecl());

        return std::move(tokens);
    }

    void addAngle(clang::SourceLocation left, clang::SourceLocation right) {}

    void TraverseTokens() {
        auto spelledTokens = tokenBuffer.spelledTokens(sourceManager.getMainFileID());
        bool in_directive = false;
        bool in_include = false;
        for(std::size_t index = 0; index < spelledTokens.size(); ++index) {
            auto& current = spelledTokens[index];
            switch(current.kind()) {
                case clang::tok::TokenKind::comment: {
                    tokens.emplace_back(SemanticTokenType::Comment, current);
                    break;
                }
                case clang::tok::TokenKind::numeric_constant: {
                    tokens.emplace_back(SemanticTokenType::Number, current);
                    break;
                }
                case clang::tok::TokenKind::char_constant:
                case clang::tok::TokenKind::wide_char_constant:
                case clang::tok::TokenKind::utf8_char_constant:
                case clang::tok::TokenKind::utf16_char_constant:
                case clang::tok::TokenKind::utf32_char_constant: {
                    tokens.emplace_back(SemanticTokenType::Char, current);
                    break;
                }
                case clang::tok::TokenKind::string_literal:
                case clang::tok::TokenKind::wide_string_literal:
                case clang::tok::TokenKind::utf8_string_literal:
                case clang::tok::TokenKind::utf16_string_literal:
                case clang::tok::TokenKind::utf32_string_literal: {
                    tokens.emplace_back(SemanticTokenType::String, current);
                    break;
                }
                case clang::tok::identifier: {
                    break;
                }
                case clang::tok::l_paren: {
                    tokens.emplace_back(SemanticTokenType::Paren, current)
                        .addModifier(SemanticTokenModifier::Left);
                    break;
                }
                case clang::tok::r_paren: {
                    tokens.emplace_back(SemanticTokenType::Paren, current)
                        .addModifier(SemanticTokenModifier::Right);
                    break;
                }
                case clang::tok::l_square: {
                    tokens.emplace_back(SemanticTokenType::Bracket, current)
                        .addModifier(SemanticTokenModifier::Left);
                    break;
                }
                case clang::tok::r_square: {
                    tokens.emplace_back(SemanticTokenType::Bracket, current)
                        .addModifier(SemanticTokenModifier::Right);
                    break;
                }
                case clang::tok::l_brace: {
                    tokens.emplace_back(SemanticTokenType::Brace, current)
                        .addModifier(SemanticTokenModifier::Left);
                    break;
                }
                case clang::tok::r_brace: {
                    tokens.emplace_back(SemanticTokenType::Brace, current)
                        .addModifier(SemanticTokenModifier::Right);
                    break;
                }
                case clang::tok::plus:          // +
                case clang::tok::plusplus:      // ++
                case clang::tok::plusequal:     // +=
                case clang::tok::minus:         // -
                case clang::tok::minusminus:    // --
                case clang::tok::minusequal:    // -=
                case clang::tok::star:          // *
                case clang::tok::starequal:     // *=
                case clang::tok::slash:         // /
                case clang::tok::slashequal:    // /=
                case clang::tok::percent:       // %
                case clang::tok::percentequal:  // %=
                case clang::tok::amp:           // &
                case clang::tok::ampamp:        // &&
                case clang::tok::ampequal:      // &=
                case clang::tok::pipe:          // |
                case clang::tok::pipepipe:      // ||
                case clang::tok::pipeequal:     // |=
                case clang::tok::caret:         // ^
                case clang::tok::caretequal:    // ^=
                case clang::tok::exclaim:       // !
                case clang::tok::exclaimequal:  // !=
                case clang::tok::equal:         // =
                case clang::tok::equalequal:    // ==
                {
                }
                case clang::tok::eod: {
                    in_directive = false;
                    break;
                }
                default: break;
            }
        }
    }

    Traverse(TranslationUnitDecl) {
        for(auto decl: node->decls()) {
            // we only need to highlight the token in main file.
            // so filter out the nodes which are in headers for better performance.
            if(sourceManager.isInMainFile(decl->getLocation())) {
                TraverseDecl(decl);
            }
        }
        return true;
    }

    WalkUpFrom(NamespaceDecl) {}

    VISIT(DeclaratorDecl) {
        for(unsigned i = 0; i < node->getNumTemplateParameterLists(); ++i) {
            if(auto params = node->getTemplateParameterList(i)) {
                addAngle(params->getLAngleLoc(), params->getRAngleLoc());
            }
        }
        return true;
    }

    VISIT(TagDecl) {
        for(unsigned i = 0; i < node->getNumTemplateParameterLists(); ++i) {
            if(auto params = node->getTemplateParameterList(i)) {
                addAngle(params->getLAngleLoc(), params->getRAngleLoc());
            }
        }
        return true;
    }

    VISIT(FunctionDecl) {
        if(auto args = node->getTemplateSpecializationArgsAsWritten()) {
            addAngle(args->getLAngleLoc(), args->getRAngleLoc());
        }
        return true;
    }

    VISIT(TemplateDecl) {
        if(auto params = node->getTemplateParameters()) {
            addAngle(params->getLAngleLoc(), params->getRAngleLoc());
        }
        return true;
    }

    VISIT(ClassTemplateSpecializationDecl) {
        if(auto args = node->getTemplateArgsAsWritten()) {
            addAngle(args->getLAngleLoc(), args->getRAngleLoc());
        }
        return true;
    }

    VISIT(ClassTemplatePartialSpecializationDecl) {
        if(auto params = node->getTemplateParameters()) {
            addAngle(params->getLAngleLoc(), params->getRAngleLoc());
        }
        return true;
    }

    VISIT(VarTemplateSpecializationDecl) {
        if(auto args = node->getTemplateArgsAsWritten()) {
            addAngle(args->LAngleLoc, args->RAngleLoc);
        }
        return true;
    }

    VISIT(VarTemplatePartialSpecializationDecl) {
        if(auto params = node->getTemplateParameters()) {
            addAngle(params->getLAngleLoc(), params->getRAngleLoc());
        }
        return true;
    }

    VISIT(CXXNamedCastExpr) {
        addAngle(node->getAngleBrackets().getBegin(), node->getAngleBrackets().getEnd());
        return true;
    }

    VISIT(OverloadExpr) {
        addAngle(node->getLAngleLoc(), node->getRAngleLoc());
        return true;
    }

    VISIT(CXXDependentScopeMemberExpr) {
        addAngle(node->getLAngleLoc(), node->getRAngleLoc());
        return true;
    }

    VISIT(DependentScopeDeclRefExpr) {
        addAngle(node->getLAngleLoc(), node->getRAngleLoc());
        return true;
    }

    VISIT_TYPE(TemplateSpecializationTypeLoc) {
        addAngle(node.getLAngleLoc(), node.getRAngleLoc());
        return true;
    }

    VISIT_TYPE(DependentTemplateSpecializationTypeLoc) {
        addAngle(node.getLAngleLoc(), node.getRAngleLoc());
        return true;
    }
};

#undef Traverse;
#undef WalkUpFrom;
#undef VISIT;

}  // namespace clice::feature
