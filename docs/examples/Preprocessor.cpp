#include <Clang/Clang.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/MacroArgs.h>

using namespace clang;

struct IfBlock {
    SourceLocation If;
    std::vector<SourceLocation> Elifs;
    SourceLocation Else;
    SourceLocation End;
};

struct IfdefBlock {
    SourceLocation If;
    std::vector<SourceLocation> Elifs;
    SourceLocation Else;
    SourceLocation End;
};

class DirectiveCollector : public clang::PPCallbacks {
private:
    clang::Preprocessor& pp;
    clang::SourceManager& sm;

public:
    DirectiveCollector(clang::Preprocessor& pp) : pp(pp), sm(pp.getSourceManager()) {}

    virtual ~DirectiveCollector() = default;

    void InclusionDirective(clang::SourceLocation HashLoc,
                            const clang::Token& IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            clang::CharSourceRange FilenameRange,
                            clang::OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const clang::Module* SuggestedModule,
                            bool ModuleImported,
                            clang::SrcMgr::CharacteristicKind FileType) override {}

    void moduleImport(clang::SourceLocation ImportLoc,
                      clang::ModuleIdPath Path,
                      const clang::Module* Imported) override {}

    void PragmaDirective(clang::SourceLocation Loc,
                         clang::PragmaIntroducerKind Introducer) override {}

    void MacroExpands(const clang::Token& MacroNameTok,
                      const clang::MacroDefinition& MD,
                      clang::SourceRange Range,
                      const clang::MacroArgs* Args) override {}

    void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override {}

    void MacroUndefined(const clang::Token& MacroNameTok,
                        const clang::MacroDefinition& MD,
                        const clang::MacroDirective* Undef) override {}

    void If(clang::SourceLocation Loc,
            clang::SourceRange ConditionRange,
            ConditionValueKind ConditionValue) override {
        llvm::outs() << "If\n";
        Loc.dump(sm);
        ConditionRange.dump(sm);
    }

    void Elif(clang::SourceLocation Loc,
              clang::SourceRange ConditionRange,
              ConditionValueKind ConditionValue,
              clang::SourceLocation IfLoc) override {}

    void Else(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override {}

    void Ifdef(clang::SourceLocation Loc,
               const clang::Token& MacroNameTok,
               const clang::MacroDefinition& MD) override {
        llvm::outs() << "Ifdef\n";
        Loc.dump(sm);

        llvm::outs() << "MacroName: " << pp.getSpelling(MacroNameTok) << "\n";
        auto info = MD.getMacroInfo();
        if(info) {
            info->dump();
        }
    }

    void Elifdef(clang::SourceLocation Loc,
                 const clang::Token& MacroNameTok,
                 const clang::MacroDefinition& MD) override {}

    void Ifndef(clang::SourceLocation Loc,
                const clang::Token& MacroNameTok,
                const clang::MacroDefinition& MD) override {}

    void Elifndef(clang::SourceLocation Loc,
                  const clang::Token& MacroNameTok,
                  const clang::MacroDefinition& MD) override {}

    void Endif(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override {
        llvm::outs() << "Endif\n";
        Loc.dump(sm);
        IfLoc.dump(sm);
    }
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

    pp.addPPCallbacks(std::make_unique<DirectiveCollector>(pp));
    clang::syntax::TokenCollector collector{pp};

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto buffer = std::move(collector).consume();
    buffer.dumpForTests();
    auto tokens = buffer.spelledTokens(sm.getMainFileID());
    // for(auto& token: tokens) {
    //
    //    llvm::outs() << "Token: " << token.text(sm) << " " <<
    //    clang::tok::getTokenName(token.kind())
    //                 << "\n";
    //}

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
