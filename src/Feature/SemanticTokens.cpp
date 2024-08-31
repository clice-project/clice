#include "AST/ParsedAST.h"
#include "Feature/SemanticTokens.h"

namespace clice::feature {

namespace {

#define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
#define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)
#define VISIT_TYPE(NAME) bool Visit##NAME(clang::NAME node)

class SemanticToken {};

class Highlighter : public clang::RecursiveASTVisitor<Highlighter> {
public:
    Highlighter(ParsedAST& ast) : fileManager(ast.fileManager), preproc(ast.preproc), sourceManager(ast.sourceManager), context(ast.context), tokenBuffer(ast.tokenBuffer) {}

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
            // we only need to highlight the token in main file.
            // so filter out the nodes which are in headers for better performance.
            if(sourceManager.isInFileID(decl->getLocation(), fileID)) {
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

}  // namespace clice::feature
