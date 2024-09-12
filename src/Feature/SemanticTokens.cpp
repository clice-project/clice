#include "AST/ParsedAST.h"
#include "Feature/SemanticTokens.h"

namespace clice::feature {

namespace {

#define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
#define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)
#define VISIT_TYPE(NAME) bool Visit##NAME(clang::NAME node)

struct SemanticToken {
    std::uint32_t modifiers = 0;
    clang::SourceRange range;
    protocol::SemanticTokenType type;
};

static bool isKeyword(clang::tok::TokenKind kind, llvm::StringRef text, const clang::LangOptions& option) {
    switch(kind) {
        case clang::tok::kw_void:
        case clang::tok::kw_int:
        case clang::tok::kw_char:
        case clang::tok::kw_long:
        case clang::tok::kw_short:
        case clang::tok::kw_signed:
        case clang::tok::kw_unsigned:
        case clang::tok::kw_float:
        case clang::tok::kw_double:
        case clang::tok::kw_const:
        case clang::tok::kw_volatile:
        case clang::tok::kw_auto:
        case clang::tok::kw_static:
        case clang::tok::kw_register:
        case clang::tok::kw_extern:
        case clang::tok::kw_if:
        case clang::tok::kw_else:
        case clang::tok::kw_switch:
        case clang::tok::kw_case:
        case clang::tok::kw_default:
        case clang::tok::kw_do:
        case clang::tok::kw_for:
        case clang::tok::kw_while:
        case clang::tok::kw_break:
        case clang::tok::kw_continue:
        case clang::tok::kw_goto:
        case clang::tok::kw_return:
        case clang::tok::kw_struct:
        case clang::tok::kw_union:
        case clang::tok::kw_enum:
        case clang::tok::kw_typedef:
        case clang::tok::kw_sizeof: {
            return true;
        }

        case clang::tok::amp: return option.CPlusPlus && text == "bitand";
        case clang::tok::ampamp: return option.CPlusPlus && text == "and";
        case clang::tok::ampequal: return option.CPlusPlus && text == "and_eq";
        case clang::tok::pipe: return option.CPlusPlus && text == "bitor";
        case clang::tok::pipepipe: return option.CPlusPlus && text == "or";
        case clang::tok::pipeequal: return option.CPlusPlus && text == "or_eq";
        case clang::tok::caret: return option.CPlusPlus && text == "xor";
        case clang::tok::caretequal: return option.CPlusPlus && text == "xor_eq";
        case clang::tok::exclaim: return option.CPlusPlus && text == "not";
        case clang::tok::exclaimequal: return option.CPlusPlus && text == "not_eq";
        case clang::tok::tilde: return option.CPlusPlus && text == "compl";

        case clang::tok::kw_asm:
        case clang::tok::kw_wchar_t:
        case clang::tok::kw_try:
        case clang::tok::kw_throw:
        case clang::tok::kw_catch:
        case clang::tok::kw_typeid:
        case clang::tok::kw_this:
        case clang::tok::kw_friend:
        case clang::tok::kw_mutable:
        case clang::tok::kw_explicit:
        case clang::tok::kw_virtual:
        case clang::tok::kw_operator:
        case clang::tok::kw_class:
        case clang::tok::kw_public:
        case clang::tok::kw_protected:
        case clang::tok::kw_private:
        case clang::tok::kw_using:
        case clang::tok::kw_namespace:
        case clang::tok::kw_template:
        case clang::tok::kw_typename:
        case clang::tok::kw_export:
        case clang::tok::kw_const_cast:
        case clang::tok::kw_static_cast:
        case clang::tok::kw_dynamic_cast:
        case clang::tok::kw_reinterpret_cast:
        case clang::tok::kw_new:
        case clang::tok::kw_delete: {
            return option.CPlusPlus;
        }

        case clang::tok::kw_restrict:
        case clang::tok::kw__Bool:
        case clang::tok::kw__Complex:
        case clang::tok::kw__Imaginary: {
            return option.C99;
        }

        case clang::tok::kw_inline: {
            return option.CPlusPlus || option.C99;
        }

        case clang::tok::kw__Alignas:
        case clang::tok::kw__Alignof:
        case clang::tok::kw__Atomic:
        case clang::tok::kw__Generic:
        case clang::tok::kw__Noreturn:
        case clang::tok::kw__Static_assert:
        case clang::tok::kw__Thread_local: {
            return option.C11;
        }

        case clang::tok::kw_typeof:
        case clang::tok::kw_typeof_unqual:
        case clang::tok::kw__BitInt:
        case clang::tok::kw__Decimal32:
        case clang::tok::kw__Decimal64:
        case clang::tok::kw__Decimal128: {
            return option.C23;
        }

        case clang::tok::kw_bool:
        case clang::tok::kw_true:
        case clang::tok::kw_false: {
            return option.CPlusPlus || option.C23;
        }

        case clang::tok::kw_char16_t:
        case clang::tok::kw_char32_t:
        case clang::tok::kw_noexcept:
        case clang::tok::kw_decltype: {
            return option.CPlusPlus11;
        }

        case clang::tok::kw_nullptr:
        case clang::tok::kw_alignas:
        case clang::tok::kw_alignof:
        case clang::tok::kw_thread_local:
        case clang::tok::kw_static_assert:
        case clang::tok::kw_constexpr: {
            return option.CPlusPlus11 || option.C23;
        }

        case clang::tok::kw_char8_t:
        case clang::tok::kw_import:
        case clang::tok::kw_module:
        case clang::tok::kw_constinit:
        case clang::tok::kw_consteval:
        case clang::tok::kw_concept:
        case clang::tok::kw_requires:
        case clang::tok::kw_co_await:
        case clang::tok::kw_co_yield:
        case clang::tok::kw_co_return: {
            return option.CPlusPlus20;
        }
    }
    return false;
}

class HighlightBuilder {
public:
    HighlightBuilder(const ParsedAST& parsedAST, llvm::StringRef filename) {
        auto fileID = parsedAST.getFileID(filename);
        auto tokens = parsedAST.spelledTokens(fileID);
        for(auto& token: tokens) {
            SemanticToken semanticToken;
            semanticToken.range = clang::SourceRange(token.location(), token.endLocation());
            if(isKeyword(token.kind(), token.text(parsedAST.sourceManager), parsedAST.context.getLangOpts())) {
                semanticToken.type = protocol::SemanticTokenType::Keyword;
            } else {
                semanticToken.type = protocol::SemanticTokenType::Number;
            }
            semanticToken.modifiers = 0;
            result.emplace_back(std::move(semanticToken));
        }
    }

    std::vector<SemanticToken> build() { return std::move(result); }

private:
    std::vector<SemanticToken> result;
};

class Highlighter : public clang::RecursiveASTVisitor<Highlighter> {
public:
    Highlighter(ParsedAST& ast) :
        fileManager(ast.fileManager), preproc(ast.preproc), sourceManager(ast.sourceManager), context(ast.context),
        tokenBuffer(ast.tokenBuffer) {}

    std::vector<SemanticToken> highlight(llvm::StringRef filepath) {
        std::vector<SemanticToken> result;

        auto entry = fileManager.getFileRef(filepath);
        if(auto error = entry.takeError()) {
            // TODO:
        }
        auto fileID = sourceManager.translateFile(entry.get());

        this->fileID = fileID;
        this->result = &result;

        // highlight from tokens
        // TODO: use TokenBuffer to get tokens

        // TODO: highlight from directive

        // highlight from AST
        TraverseDecl(context.getTranslationUnitDecl());

        return {};
    }

private:
    void addAngle(clang::SourceLocation left, clang::SourceLocation right) {}

public:
    Traverse(TranslationUnitDecl) {
        for(auto decl: node->decls()) {
            // FIXME: some decls are located in their parents' file
            // e.g. `ClassTemplateSpecializationDecl`, find and exclude them
            node->getLexicalDeclContext();
            if(sourceManager.isInFileID(decl->getLocation(), fileID)) {
                TraverseDecl(decl);
            }
        }
        return true;
    }

    WalkUpFrom(NamespaceDecl) {}

    VISIT(ImportDecl) {}

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

private:
    clang::FileManager& fileManager;
    clang::Preprocessor& preproc;
    clang::SourceManager& sourceManager;
    clang::ASTContext& context;
    clang::syntax::TokenBuffer& tokenBuffer;

    clang::FileID fileID;
    std::vector<SemanticToken>* result;
};

}  // namespace

protocol::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename) {
    HighlightBuilder builder(AST, filename);
    std::vector<SemanticToken> tokens = builder.build();

    protocol::SemanticTokens result;
    unsigned int last_line = 0;
    unsigned int last_column = 0;
    /// FXIME: resolve position encoding
    for(std::size_t index = 0; index < tokens.size(); ++index) {
        auto& token = tokens[index];
        auto line = AST.sourceManager.getPresumedLineNumber(token.range.getBegin()) - 1;
        auto column = AST.sourceManager.getPresumedColumnNumber(token.range.getBegin()) - 1;
        auto length = AST.sourceManager.getPresumedColumnNumber(token.range.getEnd()) - column;
        result.data.push_back(line - last_line);
        result.data.push_back(column > last_column ? column - last_column : column);
        result.data.push_back(length);
        result.data.push_back(token.type);
        result.data.push_back(token.modifiers);

        last_line = line;
        last_column = column;
    }

    return result;
};

}  // namespace clice::feature
