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

#include "Server/Tidy.h"

namespace clice::tidy {

using namespace clang::tidy;

bool isRegisteredTidyCheck(llvm::StringRef Check) {
    assert(!Check.empty());
    assert(!Check.contains('*') && !Check.contains(',') &&
           "isRegisteredCheck doesn't support globs");
    assert(Check.ltrim().front() != '-');

    const static llvm::StringSet<llvm::BumpPtrAllocator> AllChecks = [] {
        llvm::StringSet<llvm::BumpPtrAllocator> Result;
        tidy::ClangTidyCheckFactories Factories;
        for(tidy::ClangTidyModuleRegistry::entry E: tidy::ClangTidyModuleRegistry::entries())
            E.instantiate()->addCheckFactories(Factories);
        for(const auto& Factory: Factories)
            Result.insert(Factory.getKey());
        return Result;
    }();

    return AllChecks.contains(Check);
}

std::optional<bool> isFastTidyCheck(llvm::StringRef Check) {
    static auto& Fast = *new llvm::StringMap<bool>{
#define FAST(CHECK, TIME) {#CHECK, true},
#define SLOW(CHECK, TIME) {#CHECK, false},
// todo: move me to llvm toolchain headers.
#include "TidyFastChecks.inc"
    };
    if(auto It = Fast.find(Check); It != Fast.end())
        return It->second;
    return std::nullopt;
}

}  // namespace clice::tidy
