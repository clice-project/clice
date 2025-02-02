#pragma once

#include "clang/Basic/Diagnostic.h"

namespace clice {

class DiagnosticCollector : public clang::DiagnosticConsumer {
public:
    void BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) override;

    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic& Info) override;

    void EndSourceFile() override;
};

}  // namespace clice
