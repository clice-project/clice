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

    using namespace clang;

    class Callback : public clang::PPCallbacks {

    public:
        Preprocessor& pp;
        SourceRange last;

        Callback(Preprocessor& pp) : pp(pp) {}

        /// Called by Preprocessor::HandleMacroExpandedIdentifier when a
        /// macro invocation is found.
        void MacroExpands(const Token& MacroNameTok,
                          const MacroDefinition& MD,
                          SourceRange Range,
                          const MacroArgs* Args) override {

            auto name = MacroNameTok.getIdentifierInfo()->getName();
            if(name.starts_with("__"))
                return;
            llvm::outs() << "MacroExpands: " << name;
            if(MD.getMacroInfo()->isFunctionLike()) {
                llvm::outs() << "(";
                int len = Args->getNumMacroArguments();
                for(auto i = 0; i < len; i++) {
                    auto arg = Args->getUnexpArgument(i);
                    auto len2 = Args->getArgLength(arg);
                    for(auto j = 0; j < len2; j++) {
                        llvm::outs() << pp.getSpelling(*(arg + j));
                    }
                    if(i < len - 1)
                        llvm::outs() << ", ";
                }
                llvm::outs() << ")";
            }
            llvm::outs() << "\n";
            auto& m = pp.getSourceManager();
            auto x = m.getExpansionRange(Range);
            // auto z = m.getImmediateExpansionRange(x.getBegin());

            auto text = Lexer::getSourceText(x, m, pp.getLangOpts());
            llvm::outs() << text << "\n";
        }

        /// Hook called whenever a macro definition is seen.
        void MacroDefined(const Token& MacroNameTok, const MacroDirective* MD) override {
            auto name = MacroNameTok.getIdentifierInfo()->getName();
            if(name.starts_with("__"))
                return;
            // llvm::outs() << "MacroDefined: " << name << "\n";
        }
    };

    /// if code completion
    // instance->setCodeCompletionConsumer(consumer);
    SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    // must be after BeginSourceFile
    auto& preprocessor = instance->getPreprocessor();
    preprocessor.addPPCallbacks(std::make_unique<Callback>(preprocessor));

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    // instance->getASTContext().getTranslationUnitDecl()->dump();

    action.EndSourceFile();
}
