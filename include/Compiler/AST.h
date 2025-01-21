#pragma once

#include "Diagnostic.h"
#include "Resolver.h"
#include "Directive.h"

#include "clang/Frontend/CompilerInstance.h"

namespace clice {

/// All AST related information needed for language server.
class ASTInfo {
public:
    ASTInfo(clang::FileID interested, std::unique_ptr<clang::FrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::optional<TemplateResolver> resolver,
            std::optional<clang::syntax::TokenBuffer> buffer,
            llvm::DenseMap<clang::FileID, Directive> directives) :
        interested(interested), action(std::move(action)), instance(std::move(instance)),
        m_resolver(std::move(resolver)), buffer(std::move(buffer)), m_diagnostics(std::nullopt),
        m_directives(std::move(directives)) {}

    ASTInfo(const ASTInfo&) = delete;

    ASTInfo(ASTInfo&&) = default;

    ~ASTInfo() {
        if(action) {
            action->EndSourceFile();
        }
    }

public:
    auto& srcMgr() {
        return instance->getSourceManager();
    }

    auto& pp() {
        return instance->getPreprocessor();
    }

    auto& context() {
        return instance->getASTContext();
    }

    auto& sema() {
        return instance->getSema();
    }

    auto& tokBuf() {
        assert(buffer && "Token buffer is not available");
        return *buffer;
    }

    auto& resolver() {
        assert(m_resolver && "Template resolver is not available");
        return *m_resolver;
    }

    auto& directives() {
        return m_directives;
    }

    auto tu() {
        return instance->getASTContext().getTranslationUnitDecl();
    }

    auto& diagnostics() {
        assert(m_diagnostics && "Diagnostics is not available");
        return *m_diagnostics;
    }

    /// TODO:
    /// Remove this workaround setter.
    void setDiagnostics(std::vector<Diagnostic> diagnostics) {
        assert(!m_diagnostics.has_value() && "Diagnostics has been set");
        m_diagnostics = std::move(diagnostics);
    }

    std::vector<std::string> deps();

private:
    /// The interested file ID. For file without header context, it is the main file ID.
    /// For file with header context, it is the file ID of header file.
    clang::FileID interested;

    /// The frontend action used to build the AST.
    std::unique_ptr<clang::FrontendAction> action;

    /// Compiler instance, responsible for performing the actual compilation and managing the
    /// lifecycle of all objects during the compilation process.
    std::unique_ptr<clang::CompilerInstance> instance;

    /// The template resolver used to resolve dependent name.
    std::optional<TemplateResolver> m_resolver;

    /// Token information collected during the preprocessing.
    std::optional<clang::syntax::TokenBuffer> buffer;

    /// Diagnostics collected during the preprocessing.
    std::optional<std::vector<Diagnostic>> m_diagnostics;

    /// All diretive information collected during the preprocessing.
    llvm::DenseMap<clang::FileID, Directive> m_directives;
};

}  // namespace clice
