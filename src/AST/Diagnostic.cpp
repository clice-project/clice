#include "AST/Diagnostic.h"

namespace clice {

void Diagnostic::BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) {

};

void Diagnostic::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& diagnostic) {
    llvm::SmallString<128> message;
    diagnostic.FormatDiagnostic(message);
    // diagnostic.getLocation();
    llvm::outs() << "Diagnostic: " << message << "\n";
};

void Diagnostic::EndSourceFile() {

};

}  // namespace clice
