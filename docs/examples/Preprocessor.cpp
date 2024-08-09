#include <Clang/Clang.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/MacroArgs.h>

using namespace clang;

class PPCallback : public clang::PPCallbacks {
private:
    clang::Preprocessor& pp;
    clang::SourceManager& sm;

public:
    PPCallback(clang::Preprocessor& pp) : pp(pp), sm(pp.getSourceManager()) {}

    void InclusionDirective(SourceLocation HashLoc,
                            const Token& IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const Module* SuggestedModule,
                            bool ModuleImported,
                            SrcMgr::CharacteristicKind FileType) override {
        if(sm.isInMainFile(HashLoc)) {
            HashLoc.dump(sm);
            IncludeTok.getLocation().dump(sm);
            IncludeTok.getEndLoc().dump(sm);
        }
    }

    // void MacroExpands(const clang::Token& token,
    //                   const clang::MacroDefinition& macro,
    //                   clang::SourceRange range,
    //                   const clang::MacroArgs* args) override {
    //     std::string name = pp.getSpelling(token);
    //     if(name.starts_with("_"))
    //         return;
    //
    //    clang::MacroInfo* info = macro.getMacroInfo();
    //    // info->isBuiltinMacro();
    //    // info->isFunctionLike();
    //    // info->isObjectLike();
    //    // info->isVariadic();
    //    // info->getNumParams();
    //    // info->params();
    //
    //    const int size = args->getNumMacroArguments();  // Expanding macro arguments
    //    for(auto i = 0; i < size; ++i) {
    //        // get first token of first argument of expanding macro
    //        const clang::Token* first = args->getUnexpArgument(i);
    //        // iterate over tokens of first argument of expanding macro
    //        for(auto j = 0; j < args->getArgLength(first); ++j) {
    //            const clang::Token& tok = *(first + j);
    //            //  llvm::outs() << "Arg: " << pp.getSpelling(tok) << "\n";
    //        }
    //    }
    //
    //    auto expandingRange = sm.getExpansionRange(range);
    //    auto text = clang::Lexer::getSourceText(expandingRange, sm, pp.getLangOpts());
    //    llvm::outs() << text << "\n";
    //}

    void MacroDefined(const clang::Token& token, const clang::MacroDirective* directive) override {}
};

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preprocessor <source-file>");
    llvm::outs() << "running Preprocessor...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "/usr/local/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        argv[1],
    };

    invocation = clang::createInvocation(args, {});

    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    clang::PreprocessOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    clang::Preprocessor& pp = instance->getPreprocessor();
    auto& sm = pp.getSourceManager();

    // pp.setTokenWatcher([&](const clang::Token& token) {
    //     llvm::outs() << "Token: " << pp.getSpelling(token);
    //     auto loc = sm.getSpellingLoc(token.getLocation());
    //
    //     //llvm::outs() << " at " << pp.get << "\n";
    //     // if(!token.isAnnotation()) {
    //     //     if(auto II = token.getIdentifierInfo()) {
    //     //         if(II->isKeyword(pp.getLangOpts())) {
    //     //             auto name = clang::Lexer::getSpelling(token, pp.getSourceManager(),
    //     pp.getLangOpts());
    //     //             llvm::outs() << "Keyword: " << name << "\n";
    //     //         }
    //     //     }
    //     // } else {
    //     //     // TODO: split annoated token
    //     // }
    // });
    pp.addPPCallbacks(std::make_unique<PPCallback>(pp));
    clang::syntax::TokenCollector collector{pp};

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto buffer = std::move(collector).consume();
    buffer.dumpForTests();
    auto tokens = buffer.spelledTokens(sm.getMainFileID());
    for(auto& token: tokens) {

        llvm::outs() << "Token: " << token.text(sm) << " " << clang::tok::getTokenName(token.kind())
                     << "\n";
    }
    buffer.expansionStartingAt(tokens[0].location());
    // auto tokens2 = buffer.expandedTokens();
    // for(auto& token: tokens2) {
    //     token.dumpForTests(sm);
    // }
    //  auto buffer = sm.getBufferData(sm.getMainFileID());
    //  llvm::outs() << buffer << "\n";
    //  buffer.spelledTokenContaining()
    //  all operations should before action end
    action.EndSourceFile();
}
