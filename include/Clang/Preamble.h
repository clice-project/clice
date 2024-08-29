#pragma once

#include "Directive.h"

namespace clice {

class Preamble {
private:
    Directive directive;
    clang::PrecompiledPreamble data;

    // Preamble(clang::PrecompiledPreamble&& preamble) : data(std::move(preamble)) {}

public:
    static std::unique_ptr<Preamble>
        build(PathRef path, StringRef content, const CompilerInvocation& invocation);
};

}  // namespace clice
