#pragma once

#include <chrono>

#include "libuv.h"
#include "Task.h"
#include "Awaiter.h"

#include "Support/JSON.h"
#include "Support/Enum.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::async {

namespace fs {

struct Mode : refl::Enum<Mode, true> {
    enum Kind {
        /// Open the file for reading.
        Read = 0,

        /// Open the file for writing.
        Write,

        /// Open the file for reading and writing.
        ReadWrite,

        /// If the file does not exist, create it.
        Create,

        /// If the file exists, append the data to the end of the file.
        Append,

        /// If the file exists, truncate the file to zero length.
        Truncate,

        /// If the file exists, fail the open.
        Exclusive,
    };

    using Enum::Enum;
};

struct handle {
    uv_file file;

    ~handle();
};

/// Open the file asynchronously.
Result<handle> open(std::string path, Mode mode);

/// Read the file asynchronously, make sure the buffer is valid until the task is done.
Result<ssize_t> read(handle handle, char* buffer, std::size_t size);

Result<std::string> read(std::string path, Mode mode = Mode::Read);

/// Write the file asynchronously, make sure the buffer is valid until the task is done.
Result<void> write(handle handle, char* buffer, std::size_t size);

Result<void> write(std::string path,
                   char* buffer,
                   std::size_t size,
                   Mode mode = Mode(Mode::Write, Mode::Create, Mode::Truncate));

struct Stats {
    std::chrono::milliseconds mtime;
};

Result<Stats> stat(std::string path);

}  // namespace fs

}  // namespace clice::async

