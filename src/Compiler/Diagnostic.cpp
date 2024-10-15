#include "Compiler/Diagnostic.h"

namespace clice {

void DiagnosticCollector::BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) {

};

void DiagnosticCollector::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& diagnostic) {
    llvm::SmallString<128> message;
    diagnostic.FormatDiagnostic(message);
    // diagnostic.getLocation();
    llvm::outs() << "Diagnostic: " << message << "\n";
};

void DiagnosticCollector::EndSourceFile() {

};

}  // namespace clice
