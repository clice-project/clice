#pragma once

#include "Clang.h"

namespace clice {

/// Processes and adjusts a raw compile command from compile_commands.json.
///
/// This function tokenizes the input command, removes unnecessary arguments,
/// and ensures the resulting format is suitable for execution.
///
/// @param command The raw shell-escaped compile command.
/// @param out A vector to hold pointers to the processed arguments.
/// @param buffer A storage buffer for the actual argument strings.
llvm::Error mangleCommand(llvm::StringRef command,
                          llvm::SmallVectorImpl<const char*>& out,
                          llvm::SmallVectorImpl<char>& buffer);

}  // namespace clice
