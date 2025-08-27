//===--- Run clang-tidy ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/// Partial code is copied from clangd. See:
/// https://github.com/llvm/llvm-project//blob/0865ecc5150b9a55ba1f9e30b6d463a66ac362a6/clang-tools-extra/clangd/ParsedAST.cpp#L547
/// https://github.com/llvm/llvm-project//blob/0865ecc5150b9a55ba1f9e30b6d463a66ac362a6/clang-tools-extra/clangd/TidyProvider.cpp

#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "clang-tidy/ClangTidyOptions.h"

#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Allocator.h"

#include "Compiler/Tidy.h"

namespace clice::tidy {

using namespace clang::tidy;

bool isRegisteredTidyCheck(llvm::StringRef check) {
    assert(!check.empty());
    assert(!check.contains('*') && !check.contains(',') &&
           "isRegisteredCheck doesn't support globs");
    assert(check.ltrim().front() != '-');

    const static llvm::StringSet<llvm::BumpPtrAllocator> all_checks = [] {
        llvm::StringSet<llvm::BumpPtrAllocator> result;
        tidy::ClangTidyCheckFactories factories;
        for(tidy::ClangTidyModuleRegistry::entry entry: tidy::ClangTidyModuleRegistry::entries())
            entry.instantiate()->addCheckFactories(factories);
        for(const auto& factory: factories)
            result.insert(factory.getKey());
        return result;
    }();

    return all_checks.contains(check);
}

std::optional<bool> isFastTidyCheck(llvm::StringRef check) {
    static auto& fast = *new llvm::StringMap<bool>{
#define FAST(CHECK, TIME) {#CHECK, true},
#define SLOW(CHECK, TIME) {#CHECK, false},
// todo: move me to llvm toolchain headers.
#include "TidyFastChecks.inc"
    };
    if(auto it = fast.find(check); it != fast.end())
        return it->second;
    return std::nullopt;
}

}  // namespace clice::tidy
