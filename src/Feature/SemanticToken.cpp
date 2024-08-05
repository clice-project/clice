#include <Clang/ParsedAST.h>
#include <Feature/SemanticToken.h>

namespace clice {

using SemanticTokenTypes = protocol::SemanticTokenTypes;
using SemanticTokenModifiers = protocol::SemanticTokenModifiers;

struct SemanticToken {
    clang::syntax::FileRange range;
    SemanticTokenTypes type;
    uint32_t modifiers = 0;

    SemanticToken& setType(SemanticTokenTypes type) {
        this->type = type;
        return *this;
    }

    SemanticToken& addModifier(SemanticTokenModifiers modifier) {
        modifiers |= modifier;
        return *this;
    }
};

class HighlighCollector {
private:
    clang::ASTContext& context;
    clang::syntax::TokenBuffer& buffer;
    clang::SourceManager& sourceManager;
    // store all tokens in the main file
    std::vector<SemanticToken> tokens;
    llvm::DenseMap<const clang::syntax::Token*, std::size_t> tokenMap;
    // cache the last location and token index
    // to have a better performance in traversing the same node
    clang::SourceLocation lastLocation;
    std::optional<std::size_t> lastTokenIndex;

private:
    /// determine whether the token is a keyword
    /// TODO: distinguish different language modes, e.g. C++ and C
    static bool isKeyword(clang::tok::TokenKind kind) {
        switch(kind) {
#define KEYWORD(name, ...)                                                                                   \
    case clang::tok::TokenKind::kw_##name: return true;
#include <clang/Basic/TokenKinds.def>
#undef KEYWORD
            default: return false;
        }
    }

    /// determine whether the token is an operator
    static bool isOperator(clang::tok::TokenKind kind) {
        switch(kind) {
#define PUNCTUATOR(name, ...)                                                                                \
    case clang::tok::TokenKind::name: return true;
#include <clang/Basic/TokenKinds.def>
#undef PUNCTUATOR
            default: return false;
        }
    }

public:
    HighlighCollector(ParsedAST& ast) :
        context(ast.ASTContext()), buffer(ast.TokensBuffer()), sourceManager(ast.SourceManager()) {
        // collect all source tokens(preprocessings haven't occurred)
        for(auto& token: buffer.spelledTokens(sourceManager.getMainFileID())) {
            clang::tok::TokenKind kind = token.kind();
            SemanticTokenTypes type = SemanticTokenTypes::Unknown;

            // attach basic semantic to token
            if(kind == clang::tok::numeric_constant) {
                // Numeric Literal
                type = SemanticTokenTypes::Number;
            } else if(kind == clang::tok::char_constant) {
                // Char Literal
                type = SemanticTokenTypes::Char;
            } else if(kind == clang::tok::string_literal) {
                // String Literal
                type = SemanticTokenTypes::String;
            } else if(isOperator(kind)) {
                // Operator
                type = SemanticTokenTypes::Operator;
            } else if(isKeyword(kind)) {
                // Keyword
                type = SemanticTokenTypes::Keyword;
            } else if(kind == clang::tok::comment) {
                // Comment
                type = SemanticTokenTypes::Comment;
            }

            // TODO: render diretives use the information from the preprocessor

            tokenMap[&token] = tokens.size();
            tokens.push_back({token.range(sourceManager), SemanticTokenTypes::Unknown, 0});
        }
    }

    /// determine whether the location is in the main file or in the header file
    bool isInMainFile(clang::Decl* decl) { return isInMainFile(decl->getLocation()); }

    bool isInMainFile(clang::SourceLocation location) { return sourceManager.isWrittenInMainFile(location); }

    void attach(clang::Decl* decl, SemanticTokenTypes type, uint32_t modifier);

    void attach(clang::SourceLocation location, SemanticTokenTypes type, uint32_t modifier) {
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
                tokens[index].type = type;
                tokens[index].modifiers |= modifier;
                lastTokenIndex = index;
            } else {
                // if there is no corresponding token, reset the last token index
                lastTokenIndex.reset();
            }
        } else {
            // if yes, use the last token index to find the token
            if(lastTokenIndex) {
                tokens[*lastTokenIndex].type = type;
                tokens[*lastTokenIndex].modifiers |= modifier;
            }
        }
    }

    void collect() {}
};

class HighlightVisitor : public clang::RecursiveASTVisitor<HighlightVisitor> {
private:
    HighlighCollector& collector;

public:
    HighlightVisitor(HighlighCollector& collector) : collector(collector) {}

#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)

    // filter out the nodes that are not in the main file
    VISIT(Decl) { return collector.isInMainFile(node); }

    VISIT(NamespaceDecl) {
        collector.attach(node, SemanticTokenTypes::Namespace, 0);
        return true;
    }

    VISIT(TypeDecl) {
        collector.attach(node, SemanticTokenTypes::Type, 0);
        return true;
    }

    VISIT(RecordDecl) {
        SemanticTokenTypes type;
        if(node->isUnion()) {
            type = SemanticTokenTypes::Union;
        } else if(node->isStruct()) {
            type = SemanticTokenTypes::Struct;
        } else if(node->isClass()) {
            type = SemanticTokenTypes::Class;
        }
        collector.attach(node, type, 0);
        return true;
    }

    VISIT(CXXRecordDecl) {
        collector.attach(node, SemanticTokenTypes::Class, 0);
        return true;
    }

    VISIT(FieldDecl) {
        collector.attach(node, SemanticTokenTypes::Field, 0);
        return true;
    }

    VISIT(EnumDecl) {
        collector.attach(node, SemanticTokenTypes::Enum, 0);
        return true;
    }

    VISIT(EnumConstantDecl) {
        collector.attach(node, SemanticTokenTypes::EnumMember, 0);
        return true;
    }

    VISIT(VarDecl) {
        uint32_t modifier = 0;
        modifier |= (node->isThisDeclarationADefinition() ? SemanticTokenModifiers::Definition
                                                          : SemanticTokenModifiers::Declaration);
        modifier |= (node->isConstexpr() ? SemanticTokenModifiers::Constexpr : 0);
        modifier |= (node->isInline() ? SemanticTokenModifiers::Inline : 0);
        modifier |= (node->isStaticDataMember() ? SemanticTokenModifiers::Static : 0);
        modifier |= (node->isLocalVarDecl() ? SemanticTokenModifiers::Local : 0);
        collector.attach(node, SemanticTokenTypes::Variable, modifier);
        return true;
    }

    VISIT(FunctionDecl) {
        uint32_t modifier = 0;
        modifier |= (node->isThisDeclarationADefinition() ? SemanticTokenModifiers::Definition
                                                          : SemanticTokenModifiers::Declaration);
        modifier |= (node->isVirtualAsWritten() ? SemanticTokenModifiers::Virtual : 0);
        modifier |= (node->isPureVirtual() ? SemanticTokenModifiers::PureVirtual : 0);
        modifier |= (node->isConstexpr() ? SemanticTokenModifiers::Constexpr : 0);
        modifier |= (node->isConsteval() ? SemanticTokenModifiers::Consteval : 0);
        modifier |= (node->isInlined() ? SemanticTokenModifiers::Inline : 0);
        collector.attach(node, SemanticTokenTypes::Function, modifier);
        return true;
    }

    VISIT(CXXMethodDecl) {
        collector.attach(node, SemanticTokenTypes::Method, 0);
        return true;
    }

    VISIT(ParmVarDecl) {
        collector.attach(node, SemanticTokenTypes::Parameter, 0);
        return true;
    }

    VISIT(TemplateTypeParmDecl) {
        collector.attach(node, SemanticTokenTypes::TypeTemplateParameter, 0);
        return true;
    }

    VISIT(NonTypeTemplateParmDecl) {
        collector.attach(node, SemanticTokenTypes::NonTypeTemplateParameter, 0);
        return true;
    }

    VISIT(TemplateTemplateParmDecl) {
        collector.attach(node, SemanticTokenTypes::TemplateTemplateParameter, 0);
        return true;
    }

    VISIT(ClassTemplateDecl) {
        collector.attach(node, SemanticTokenTypes::ClassTemplate, 0);
        return true;
    }

    VISIT(VarTemplateDecl) {
        collector.attach(node, SemanticTokenTypes::VariableTemplate, 0);
        return true;
    }

    VISIT(FunctionTemplateDecl) {
        collector.attach(node, SemanticTokenTypes::FunctionTemplate, 0);
        return true;
    }

    VISIT(ConceptDecl) {
        collector.attach(node, SemanticTokenTypes::Concept, 0);
        return true;
    }

    VISIT(Stmt) { return collector.isInMainFile(node->getBeginLoc()); }

    VISIT(Expr) { return collector.isInMainFile(node->getBeginLoc()); }

    VISIT(DeclRefExpr) {}

    VISIT(MemberExpr) {}

    VISIT(Attr) {
        auto location = node->getLocation();
        if(collector.isInMainFile(location)) {
            collector.attach(location, SemanticTokenTypes::Attribute, 0);
            return true;
        }
        return false;
    }

#undef VISIT
};

protocol::SemanticTokens semanticTokens(const ParsedAST& ast) {}

}  // namespace clice
