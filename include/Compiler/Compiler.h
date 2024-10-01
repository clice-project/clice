#pragma once

#include <Support/ADT.h>
#include <clang/Frontend/CompilerInstance.h>

namespace clice {

// TODO:

class Preamble;

std::unique_ptr<clang::CompilerInvocation> createInvocation(StringRef filename,
                                                            StringRef content,
                                                            std::vector<const char*>& args,
                                                            Preamble* preamble = nullptr);

std::unique_ptr<clang::CompilerInstance> createInstance(std::shared_ptr<clang::CompilerInvocation> invocation);

}  // namespace clice
