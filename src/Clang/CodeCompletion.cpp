#include <Clang/Clang.h>

namespace tooling = clang::tooling;
int line = 0;
int column = 0;
std::unique_ptr<tooling::CompilationDatabase> datebase;

class CodeCompleteConsumer : public clang::CodeCompleteConsumer {
public:
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> Allocator;
    clang::CodeCompletionTUInfo CCTUInfo;

    CodeCompleteConsumer() :
        clang::CodeCompleteConsumer(clang::CodeCompleteOptions{}),
        Allocator(std::make_shared<clang::GlobalCodeCompletionAllocator>()), CCTUInfo(Allocator) {}

    void ProcessCodeCompleteResults(clang::Sema& S,
                                    clang::CodeCompletionContext Context,
                                    clang::CodeCompletionResult* Results,
                                    unsigned NumResults) override {

        auto type = Context.getBaseType();
        type.dump();
        if(type->isDependentType()) {
            const auto dependentType = type->getAs<clang::DependentNameType>();
            // TODO: improve
        }

        for(unsigned i = 0; i < NumResults; ++i) {
            clang::CodeCompletionResult& Result = Results[i];

            switch(Result.Kind) {
                case clang::CodeCompletionResult::RK_Declaration: {
                    llvm::outs() << "Declaration: ";
                    llvm::outs() << Result.Declaration->getNameAsString() << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Keyword: {
                    llvm::outs() << "Keyword: ";
                    llvm::outs() << Result.Keyword << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Macro: {
                    llvm::outs() << "Macro: ";
                    llvm::outs() << Result.Macro->getName() << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Pattern: {
                    llvm::outs() << "Pattern: ";
                    llvm::outs() << Result.Pattern->getAsString() << "\n";
                    break;
                }
            }
        }
    }

    virtual clang::CodeCompletionAllocator& getAllocator() override { return *Allocator; }

    virtual clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() override { return CCTUInfo; }
};

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

    clang::TextDiagnosticPrinter* DiagClient = new clang::TextDiagnosticPrinter(llvm::errs(), &DiagOpts);

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

    // set code completion
    auto& completionAt = invocation->getFrontendOpts().CodeCompletionAt;
    completionAt.FileName = path.data();
    completionAt.Line = line;
    completionAt.Column = column;

    return invocation;
}

struct DiagnosticConsumer : clang::DiagnosticConsumer {
    void BeginSourceFile(const clang::LangOptions& LangOpts, const clang::Preprocessor* PP) override {}

    void EndSourceFile() override {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel, const clang::Diagnostic& Info) override {
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

    instance->setCodeCompletionConsumer(new CodeCompleteConsumer());

    return instance;
}

class AST {
    clang::FrontendAction* action;
    clang::CompilerInstance* instance;

private:
    AST() = default;

public:
    AST(const AST&) = delete;

    AST(AST&& other) noexcept : action(other.action), instance(other.instance) {
        other.action = nullptr;
        other.instance = nullptr;
    }

    ~AST() {
        if(action) {
            action->EndSourceFile();
            delete action;
            delete instance;
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

        if(llvm::Error error = ast.action->Execute()) {
            llvm::errs() << "Failed to execute action: " << error << "\n";
            std::terminate();
        }

        return ast;
    }

    auto& getASTContext() { return instance->getASTContext(); }

    auto& getSourceManager() { return instance->getSourceManager(); }
};

struct Visitor : clang::RecursiveASTVisitor<Visitor> {
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl* tu) {
        tu->dump();
        return true;
    }

    bool VisitCXXMethodDecl(clang::CXXMethodDecl* decl) { return true; }
};
