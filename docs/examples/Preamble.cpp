#include <Clang/Clang.h>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preamble <source-file>");
    llvm::outs() << "running Preamble...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation =
        std::make_shared<clang::CompilerInvocation>(std::make_shared<clang::PCHContainerOperations>());
    std::vector<const char*> args = {
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        "-Wno-everything",
        argv[1],
        "-c",
    };

    invocation = clang::createInvocation(args, {});

    // from filepath: llvm::MemoryBuffer::getFile
    // from content: llvm::MemoryBuffer::getMemBuffer(content);
    auto tmp = llvm::MemoryBuffer::getFile(argv[1]);
    if(auto error = tmp.getError()) {
        llvm::errs() << "Failed to get file: " << error.message() << "\n";
        std::terminate();
    }
    llvm::MemoryBuffer* buffer = tmp->get();

    // compute preamble bounds, if MaxLines set to false(0), it means not to limit the number of lines
    auto bounds = clang::ComputePreambleBounds({}, *buffer, false);

    auto VFS = llvm::vfs::getRealFileSystem();
    // if(auto error = VFS->setCurrentWorkingDirectory(fs::path(argv[1]).parent_path().string())) {
    //     llvm::errs() << "Failed to set current working directory: " << error.message() << "\n";
    // }

    // if store the preamble in memory, if not, store it in a file(storagePath)
    bool storeInMemory = true;
    std::string storagePath = (fs::path(argv[0]).parent_path()).string();

    // use to collect information in the process of building preamble, such as include files and macros
    // TODO: inherit from clang::PreambleCallbacks and collect the information
    clang::PreambleCallbacks callbacks = {};

    // build preamble
    auto preamble = clang::PrecompiledPreamble::Build(*invocation,
                                                      buffer,
                                                      bounds,
                                                      *engine,
                                                      VFS,
                                                      std::make_shared<clang::PCHContainerOperations>(),
                                                      storeInMemory,
                                                      "",
                                                      callbacks);

    if(auto error = preamble.getError()) {
        llvm::errs() << "Failed to build preamble: " << error.message() << "\n";
        std::terminate();
    }

    invocation = clang::createInvocation(args, {});
    // check if the preamble can be reused
    if(preamble->CanReuse(*invocation, *buffer, bounds, *VFS)) {
        llvm::outs() << "Resued preamble\n";
        // reuse preamble
        preamble->AddImplicitPreamble(*invocation, VFS, buffer);
    }

    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(clang::FileManager* manager = instance->createFileManager()) {
        instance->createSourceManager(*manager);
    } else {
        llvm::errs() << "Failed to create file manager\n";
        std::terminate();
    }

    instance->createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    // ASTContent is necessary for SemanticAnalysis
    instance->createASTContext();

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    action.EndSourceFile();
}
