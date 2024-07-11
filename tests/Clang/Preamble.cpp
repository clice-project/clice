#include <gtest/gtest.h>
#include <Clang/CompileDatabase.h>
#include <Clang/Preamble.h>
#include <Support/Filesystem.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroArgs.h>

// TEST(clice, preamble) {
//     using namespace clice;
//     CompileDatabase::instance().load("/home/ykiko/Project/C++/clice/samples/build");
//     std::string_view path = "/home/ykiko/Project/C++/clice/samples/main.cpp";
//     auto content = readAll(path);
//     CompilerInvocation invocation;
//     auto preamble = Preamble::build(path, content, invocation);
// }

TEST(clice, clang) {
    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "-x",
        "c++",
        "-no-round-trip-args",
        "-std=gnu++20",
        "/home/ykiko/Project/C++/clice/samples/main.cpp",
    };
    // invocation = clang::createInvocation(args, {});
    clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    /// if need code completion
    // auto& opts = invocation->getFrontendOpts();
    // auto& codeCompletionAt = opts.CodeCompletionAt;
    // codeCompletionAt.FileName = "main.cpp";
    // codeCompletionAt.Line = 10;
    // codeCompletionAt.Column = 4;

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(auto manager = instance->createFileManager()) {
        instance->createSourceManager(*manager);
    } else {
        llvm::errs() << "Failed to create file manager\n";
        std::terminate();
    }

    instance->createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    instance->createASTContext();

    /// if code completion
    // instance->setCodeCompletionConsumer(consumer);
    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    // instance->getASTContext().getTranslationUnitDecl()->dump();

    action.EndSourceFile();
}
