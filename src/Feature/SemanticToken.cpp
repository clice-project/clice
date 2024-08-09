#include <Clang/ParsedAST.h>
#include <Feature/SemanticToken.h>

namespace clice {

using protocol::SemanticTokenTypes;
using protocol::SemanticTokenModifiers;

struct SemanticToken {
    clang::SourceRange range;
    SemanticTokenTypes type;
    uint32_t modifiers = 0;

#define ADD_MODIFIER(NAME)                                                                         \
    SemanticToken& add##NAME(bool is##NAME) {                                                      \
        modifiers |= is##NAME ? SemanticTokenModifiers::NAME : 0;                                  \
        return *this;                                                                              \
    }
    ADD_MODIFIER(Declaration)
    ADD_MODIFIER(Definition)
    ADD_MODIFIER(Const)
    ADD_MODIFIER(Constexpr)
    ADD_MODIFIER(Consteval)
    ADD_MODIFIER(Virtual)
    ADD_MODIFIER(PureVirtual)
    ADD_MODIFIER(Inline)
    ADD_MODIFIER(Static)
    ADD_MODIFIER(Deprecated)
    ADD_MODIFIER(Local)
#undef ADD_MODIFIER

    SemanticToken& addDefinitionElseDeclaration(bool isDefinition) {
        modifiers |=
            isDefinition ? SemanticTokenModifiers::Definition : SemanticTokenModifiers::Declaration;
        return *this;
    }

    SemanticToken& setType(SemanticTokenTypes type) {
        this->type = type;
        return *this;
    }
};

class HighlighCollector {
private:
    clang::ASTContext& context;
    clang::syntax::TokenBuffer& buffer;
    clang::SourceManager& SM;
    clang::Preprocessor& PP;
    // store all tokens in the main file
    std::vector<SemanticToken> tokens;
    llvm::DenseMap<const clang::syntax::Token*, std::size_t> tokenMap;
    // cache the last location and token index
    // to have a better performance in traversing the same node
    clang::SourceLocation lastLocation;
    std::optional<std::size_t> lastTokenIndex;

public:
    HighlighCollector(ParsedAST& ast, clang::Preprocessor& PP) :
        context(ast.ASTContext()), buffer(ast.
        ()), SM(ast.SourceManager()), PP(PP) {
        // TODO: render diretives use the information from the preprocessor
        // collect all source tokens(preprocessings haven't occurred)
        for(auto& token: buffer.spelledTokens(SM.getMainFileID())) {
            clang::tok::TokenKind kind = token.kind();
            SemanticTokenTypes type = SemanticTokenTypes::Unknown;

            switch(kind) {
                case clang::tok::numeric_constant: {
                    type = SemanticTokenTypes::Number;
                    break;
                }
                case clang::tok::char_constant: {
                    type = SemanticTokenTypes::Char;
                    break;
                }
                case clang::tok::string_literal: {
                    type = SemanticTokenTypes::String;
                    break;
                }
                case clang::tok::comment: {
                    type = SemanticTokenTypes::Comment;
                    break;
                }

                    {
                        type = SemanticTokenTypes::Operator;
                        break;
                    }

                default: {
                    if(auto II = PP.getIdentifierInfo(token.text(SM))) {
                        if(II->isKeyword(PP.getLangOpts())) {
                            type = SemanticTokenTypes::Keyword;
                        }
                    }
                }
            }

            tokenMap[&token] = tokens.size();
            tokens.emplace_back(token.range(SM), type, 0);
            clang::SourceLocation loc = token.location();
            auto length = token.length();
        }
    }

    /// determine whether the location is in the main file or in the header file
    bool isInMainFile(clang::Decl* decl) { return isInMainFile(decl->getLocation()); }

    bool isInMainFile(clang::SourceLocation location) { return SM.isWrittenInMainFile(location); }

    SemanticToken* lookup(clang::SourceLocation location) {
        // check whether the location is same as the last location
        if(lastLocation != location) {
            // if not, update the last location and find the token
            lastLocation = location;
            auto token = buffer.spelledTokenContaining(location);
            if(token) {
                // if there is a corresponding token, update the token
                auto iter = tokenMap.find(token);
                assert(iter != tokenMap.end());
                auto index = iter->second;
                lastTokenIndex = index;
                return &tokens[index];
            } else {
                // if there is no corresponding token, reset the last token index
                lastTokenIndex.reset();
                return nullptr;
            }
        } else {
            // if yes, use the last token index to find the token
            if(lastTokenIndex) {
                return &tokens[*lastTokenIndex];
            }
        }
    }

    void collect() {}
};  // namespace clice

class HighlightVisitor : public clang::RecursiveASTVisitor<HighlightVisitor> {
private:
    HighlighCollector& collector;

public:
    HighlightVisitor(HighlighCollector& collector) : collector(collector) {}

#define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
#define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)

    Traverse(TranslationUnitDecl) {
        // filter out the nodes that are not in the main file(in headers)
        for(auto decl: node->decls()) {
            auto loc = decl->getLocation();
            if(collector.isInMainFile(loc)) {
                TraverseDecl(decl);
            }
        }
        return true;
    }

    WalkUpFrom(NamespaceDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Namespace);
        }
        return true;
    }

    WalkUpFrom(TypeDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Type);
        }
        return true;
    }

    WalkUpFrom(RecordDecl) {
        // if(auto token = collector.lookup(node->getLocation())) {
        //     token->setType(SemanticTokenTypes::Struct);
        // }
        return true;
    }

    WalkUpFrom(CXXRecordDecl) {
        // collector.attach(node, SemanticTokenTypes::Class, 0);
        return true;
    }

    WalkUpFrom(FieldDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Field);
            auto TSI = node->getTypeSourceInfo();
        }
        return true;
    }

    WalkUpFrom(EnumDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Enum);
        }
        return true;
    }

    WalkUpFrom(EnumConstantDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::EnumMember);
        }
        return true;
    }

    WalkUpFrom(VarDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Variable)
                .addDefinitionElseDeclaration(node->isThisDeclarationADefinition())
                .addConstexpr(node->isConstexpr())
                .addInline(node->isInline())
                .addStatic(node->isStaticDataMember())
                .addLocal(node->isLocalVarDecl());
        }
        return true;
    }

    WalkUpFrom(FunctionDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Function)
                .addDefinitionElseDeclaration(node->isThisDeclarationADefinition())
                .addConstexpr(node->isConstexpr())
                .addConsteval(node->isConsteval())
                .addVirtual(node->isVirtualAsWritten())
                .addPureVirtual(node->isPureVirtual())
                .addInline(node->isInlined());
            return true;
        }
        return false;
    }

    WalkUpFrom(CXXMethodDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Method)
                .addDefinitionElseDeclaration(node->isThisDeclarationADefinition())
                .addConstexpr(node->isConstexpr())
                .addConsteval(node->isConsteval())
                .addVirtual(node->isVirtualAsWritten())
                .addPureVirtual(node->isPureVirtual())
                .addInline(node->isInlined());
            return true;
        }
        return false;
    }

    WalkUpFrom(ParmVarDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Parameter)
                .addDefinitionElseDeclaration(node->isThisDeclarationADefinition())
                .addConst(node->isConstexpr())
                .addLocal(node->isLocalVarDecl());
        }
        return true;
    }

    WalkUpFrom(TemplateTypeParmDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::TypeTemplateParameter);
        }
        return true;
    }

    WalkUpFrom(NonTypeTemplateParmDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::NonTypeTemplateParameter).addConstexpr(true);
        }
        return true;
    }

    WalkUpFrom(TemplateTemplateParmDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::TemplateTemplateParameter);
        }
        return true;
    }

    WalkUpFrom(ClassTemplateDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::ClassTemplate);
        }
        return true;
    }

    WalkUpFrom(VarTemplateDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::VariableTemplate);
        }
        return true;
    }

    WalkUpFrom(FunctionTemplateDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::FunctionTemplate);
        }
        return true;
    }

    WalkUpFrom(ConceptDecl) {
        if(auto token = collector.lookup(node->getLocation())) {
            token->setType(SemanticTokenTypes::Concept);
        }
        return true;
    }

    WalkUpFrom(Stmt) { return collector.isInMainFile(node->getBeginLoc()); }

    WalkUpFrom(Expr) { return collector.isInMainFile(node->getBeginLoc()); }

    WalkUpFrom(DeclRefExpr) {}

    WalkUpFrom(MemberExpr) {}

    WalkUpFrom(Attr) {
        // auto location = node->getLocation();
        // if(collector.isInMainFile(location)) {
        //     collector.attach(location, SemanticTokenTypes::Attribute, 0);
        //     return true;
        // }
        return false;
    }

#undef VISIT
};

protocol::SemanticTokens semanticTokens(const ParsedAST& ast) {}

}  // namespace clice
