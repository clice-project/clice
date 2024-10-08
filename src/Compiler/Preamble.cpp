#include <Compiler/Preamble.h>

namespace clice {

class PreambleCallback : public clang::PreambleCallbacks {
public:
    std::optional<clang::syntax::TokenCollector> collector;

public:
    void BeforeExecute(clang::CompilerInstance& CI) override { collector.emplace(CI.getPreprocessor()); }

    void AfterExecute(clang::CompilerInstance& CI) override {
        auto tokens = std::move(collector.value()).consume();
        for(auto& token: tokens.expandedTokens()) {
            llvm::outs() << token.text(CI.getSourceManager()) << "\n";
        }
    }
};

std::unique_ptr<Preamble> Preamble::build(llvm::StringRef filename,
                                          llvm::StringRef content,
                                          std::vector<const char*>& args) {
    auto invocation = clang::createInvocation(args, {});
    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content, filename);

    // compute preamble bounds, i.e. the range of the preamble.
    // if MaxLines set to false(0), i.e. limit the number of lines.
    auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *buffer, false);

    // create diagnostic engine
    auto engine = clang::CompilerInstance::createDiagnostics(&invocation->getDiagnosticOpts(), new Diagnostic());

    // if store the preamble in memory, if not, store it in a file(storagePath)
    bool storeInMemory = false;

    // use to collect information in the process of building preamble, such as include files and macros
    // TODO: inherit from clang::PreambleCallbacks and collect the information
    PreambleCallback callbacks = {};

    auto preamble = clang::PrecompiledPreamble::Build(*invocation,
                                                      buffer.get(),
                                                      bounds,
                                                      *engine,
                                                      llvm::vfs::getRealFileSystem(),
                                                      std::make_shared<clang::PCHContainerOperations>(),
                                                      storeInMemory,
                                                      "",
                                                      callbacks);

    if(auto error = preamble.getError()) {
        // TODO: report error
        return nullptr;
    }

    return std::unique_ptr<Preamble>{new Preamble{std::move(preamble.get())}};
}

}  // namespace clice
