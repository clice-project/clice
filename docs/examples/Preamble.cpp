#include <filesystem>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Lex/PreprocessorOptions.h>

namespace fs = std::filesystem;

auto buildPreamble(std::vector<const char*> args, const char* path) {
    auto invocation = clang::createInvocation(args, {});

    // from filepath: llvm::MemoryBuffer::getFile
    // from content: llvm::MemoryBuffer::getMemBuffer(content);
    auto tmp = llvm::MemoryBuffer::getFile(path);
    if(auto error = tmp.getError()) {
        llvm::errs() << "Failed to get file: " << error.message() << "\n";
        std::terminate();
    }
    llvm::MemoryBuffer* buffer = tmp->release();

    // compute preamble bounds, if MaxLines set to false(0), it means not to limit the number of lines
    auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *buffer, false);

    // create diagnostic engine
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(
        new clang::DiagnosticIDs(),
        new clang::DiagnosticOptions(),
        new clang::TextDiagnosticPrinter(llvm::errs(), new clang::DiagnosticOptions()));

    // if store the preamble in memory, if not, store it in a file(storagePath)
    bool storeInMemory = false;
    std::string storagePath = (fs::path(path).parent_path() / "build").string();

    auto VFS = llvm::vfs::getRealFileSystem();
    if(auto error = VFS->setCurrentWorkingDirectory(storagePath)) {
        llvm::errs() << error.message() << "\n";
    }

    // use to collect information in the process of building preamble, such as include files and macros
    // TODO: inherit from clang::PreambleCallbacks and collect the information
    clang::PreambleCallbacks callbacks = {};

    // build preamble
    auto preamble = clang::PrecompiledPreamble::Build(*invocation,
                                                      buffer,
                                                      bounds,
                                                      *engine,
                                                      llvm::vfs::getRealFileSystem(),
                                                      std::make_shared<clang::PCHContainerOperations>(),
                                                      storeInMemory,
                                                      storagePath,
                                                      callbacks);

    if(auto error = preamble.getError()) {
        llvm::errs() << "Failed to build preamble: " << error.message() << "\n";
        std::terminate();
    }

    return preamble;
}

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preamble <source-file>");
    llvm::outs() << "running Preamble...\n";

    std::vector<const char*> args = {
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        "-Wno-everything",
        argv[1],
    };

    auto preamble = buildPreamble(args, argv[1]);

    auto instance = std::make_unique<clang::CompilerInstance>();

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    invocation = clang::createInvocation(args, {});

    auto tmp = llvm::MemoryBuffer::getFile(argv[1]);
    if(auto error = tmp.getError()) {
        llvm::errs() << "Failed to get file: " << error.message() << "\n";
        std::terminate();
    }
    llvm::MemoryBuffer* buffer = tmp->release();

    auto VFS = llvm::vfs::getRealFileSystem();
    auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *buffer, false);

    // check if the preamble can be reused
    if(preamble->CanReuse(*invocation, *buffer, bounds, *VFS)) {
        llvm::outs() << "Resued preamble\n";
        // reuse preamble
        preamble->AddImplicitPreamble(*invocation, VFS, buffer);
    }

    instance->setInvocation(std::move(invocation));

    instance->createDiagnostics(
        new clang::TextDiagnosticPrinter(llvm::errs(), new clang::DiagnosticOptions()),
        true);

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
