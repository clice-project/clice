#pragma once

#include "Clang.h"

namespace clice {

class Preamble {
private:
    clang::PrecompiledPreamble* preamble;

    Preamble(clang::PrecompiledPreamble* preamble) : preamble(preamble) {}

public:
    Preamble() = default;

    static Preamble
        build(std::string_view path, std::string_view content, const CompilerInvocation& invocation);
};

}  // namespace clice
