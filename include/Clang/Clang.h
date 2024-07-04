#pragma once

#include "clang/AST/RecursiveASTVisitor.h"
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Syntax/Tokens.h>

namespace clice {

using clang::CompilerInstance;
using clang::CompilerInvocation;

template <typename T>
union uninitialized {
    T value;

    uninitialized() {}

    ~uninitialized() { value.~T(); }

    template <typename... Args>
    auto& construct(Args&&... args) {
        return *new (&value) T{std::forward<Args>(args)...};
    }
};

}  // namespace clice
