#pragma once

#include "Compiler/CompilationUnit.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"

namespace clice {

struct CompilationUnit::Impl {
    /// The interested file ID.
    clang::FileID interested;

    clang::SourceManager& SM;

    /// The frontend action used to build the unit.
    std::unique_ptr<clang::FrontendAction> action;

    /// Compiler instance, responsible for performing the actual compilation and managing the
    /// lifecycle of all objects during the compilation process.
    std::unique_ptr<clang::CompilerInstance> instance;

    /// The template resolver used to resolve dependent name.
    std::optional<TemplateResolver> m_resolver;

    /// Token information collected during the preprocessing.
    std::optional<clang::syntax::TokenBuffer> buffer;

    /// All diretive information collected during the preprocessing.
    llvm::DenseMap<clang::FileID, Directive> m_directives;

    llvm::DenseSet<clang::FileID> allFiles;

    /// Cache for file path. It is used to avoid multiple file path lookup.
    llvm::DenseMap<clang::FileID, llvm::StringRef> pathCache;

    /// Cache for symbol id.
    llvm::DenseMap<const void*, std::uint64_t> symbolHashCache;

    llvm::BumpPtrAllocator pathStorage;

    std::vector<clang::Decl*> top_level_decls;
};

}  // namespace clice
