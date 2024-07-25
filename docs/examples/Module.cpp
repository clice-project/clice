#include <filesystem>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningTool.h>

namespace fs = std::filesystem;

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preamble <source-file>");
    llvm::outs() << "running Preamble...\n";

    std::vector<const char*> args = {
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        argv[1],
        "-c",
    };

    auto instance = std::make_unique<clang::CompilerInstance>();

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    invocation = clang::createInvocation(args, {});
    invocation->getFrontendOpts();

    auto tmp = llvm::MemoryBuffer::getFile(argv[1]);
    if(auto error = tmp.getError()) {
        llvm::errs() << "Failed to get file: " << error.message() << "\n";
        std::terminate();
    }
    llvm::MemoryBuffer* buffer = tmp->release();

    auto VFS = llvm::vfs::getRealFileSystem();
    auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *buffer, false);

    instance->setInvocation(std::move(invocation));

    instance->createDiagnostics(
        new clang::TextDiagnosticPrinter(llvm::errs(), new clang::DiagnosticOptions()),
        true);

    /// NOTICE: if preamble is stored in memory, the code below is necessary
    if(auto VFSWithRemapping =
           createVFSFromCompilerInvocation(instance->getInvocation(), instance->getDiagnostics(), VFS))
        VFS = VFSWithRemapping;
    instance->createFileManager(VFS);

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    clang::SyntaxOnlyAction action;

    auto& mainInput = instance->getFrontendOpts().Inputs[0];
    if(!action.BeginSourceFile(*instance, mainInput)) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    instance->getASTContext().getTranslationUnitDecl()->dump();

    action.EndSourceFile();
}
