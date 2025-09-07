#pragma once

#include "libuv.h"

#include "Task.h"
#include "Support/JSON.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::async::net {

using Callback = llvm::unique_function<Task<void>(json::Value)>;

/// Listen on stdin/stdout, callback is called when there is a LSP message available.
void listen(Callback callback);

/// Listen on the given ip and port, callback is called when there is a LSP message available.
void listen(const char* ip, unsigned int port, Callback callback);

/// FIXME: Spawn a new process and listen on its stdin/stdout.
void spawn(llvm::StringRef path, llvm::ArrayRef<std::string> args, Callback callback);

/// Write a JSON value to the client.
Task<> write(json::Value value);

}  // namespace clice::async::net
