#include <Clang/Clang.h>

namespace tooling = clang::tooling;

namespace {

std::unique_ptr<tooling::CompilationDatabase> datebase;

auto GetCommands(std::string_view path,
                 std::string_view compile_commands_path) -> std::vector<tooling::CompileCommand> {
    if(!datebase) {
        std::string error;
        datebase = tooling::CompilationDatabase::loadFromDirectory(compile_commands_path, error);

        if(!datebase) {
            llvm::errs() << "Failed to load compilation database. " << error << "\n";
            std::terminate();
        }
    }

    return datebase->getCompileCommands(path);
}

auto createDiagnostic() {
    clang::DiagnosticOptions DiagOpts;

    clang::TextDiagnosticPrinter* DiagClient =
        new clang::TextDiagnosticPrinter(llvm::errs(), &DiagOpts);

    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID = new clang::DiagnosticIDs();

    return clang::DiagnosticsEngine(DiagID, &DiagOpts, DiagClient);
}

auto createInvocation(std::string_view path, std::string_view compile_commands) {
    auto commands = GetCommands(path, compile_commands);
    llvm::ArrayRef command = commands[0].CommandLine;

    std::vector<const char*> args = {command.front().c_str(), "-Xclang", "-no-round-trip-args"};

    for(auto& arg: command.drop_front()) {
        args.push_back(arg.c_str());
    }

    static auto engine = createDiagnostic();
    auto invocation = clang::createInvocation(args);

    // set input file
    auto& inputs = invocation->getFrontendOpts().Inputs;
    inputs.push_back(clang::FrontendInputFile(path, clang::InputKind{clang::Language::CXX}));

    return invocation;
}

struct DiagnosticConsumer : clang::DiagnosticConsumer {
    void BeginSourceFile(const clang::LangOptions& LangOpts,
                         const clang::Preprocessor* PP) override {}

    void EndSourceFile() override {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic& Info) override {
        if(DiagLevel == clang::DiagnosticsEngine::Level::Note) {
            return;
        }

        llvm::errs() << "Diagnostic: ";
        llvm::SmallVector<char> buf;
        Info.FormatDiagnostic(buf);
        llvm::errs().write(buf.data(), buf.size());
        llvm::errs() << "\n";
    }
};

auto createInstance(std::string_view path, std::string_view compile_commands) {
    std::unique_ptr<clang::CompilerInstance> instance = std::make_unique<clang::CompilerInstance>();

    auto invocation = createInvocation(path, compile_commands);
    instance->setInvocation(std::make_shared<clang::CompilerInvocation>(*invocation));

    instance->createDiagnostics(new DiagnosticConsumer(), true);

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

    return instance;
}

class AST {
    clang::FrontendAction* action;
    clang::CompilerInstance* instance;
    clang::syntax::TokenBuffer* tokens;

private:
    AST() = default;

public:
    AST(const AST&) = delete;

    AST(AST&& other) noexcept :
        action(other.action), instance(other.instance), tokens(other.tokens) {
        other.action = nullptr;
        other.instance = nullptr;
        other.tokens = nullptr;
    }

    ~AST() {
        if(action) {
            action->EndSourceFile();
            delete action;
            delete instance;
            delete tokens;
        }
    }

    static AST create(std::string_view path, std::string_view compile_commands) {
        AST ast;

        ast.instance = createInstance(path, compile_commands).release();
        ast.action = new clang::SyntaxOnlyAction();

        const auto& input = ast.instance->getFrontendOpts().Inputs[0];

        if(!ast.action->BeginSourceFile(*ast.instance, input)) {
            llvm::errs() << "Failed to begin source file\n";
            std::terminate();
        }

        clang::syntax::TokenCollector collector = {ast.instance->getPreprocessor()};

        if(llvm::Error error = ast.action->Execute()) {
            llvm::errs() << "Failed to execute action: " << error << "\n";
            std::terminate();
        }

        ast.tokens = new auto(std::move(collector).consume());

        return ast;
    }

    auto& getASTContext() { return instance->getASTContext(); }

    auto& getSourceManager() { return instance->getSourceManager(); }

    auto getTokens() { return tokens->expandedTokens(); }
};

struct Visitor : clang::RecursiveASTVisitor<Visitor> {
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl* tu) {
        tu->dump();
        return true;
    }

    bool VisitCXXMethodDecl(clang::CXXMethodDecl* decl) { return true; }
};

}  // namespace
