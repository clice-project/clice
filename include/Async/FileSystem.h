#pragma once

#include <chrono>

#include "libuv.h"
#include "Task.h"

#include "Support/JSON.h"
#include "Support/Enum.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::async {

namespace fs {

using handle = uv_file;

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

/// Open the file asynchronously.
[[nodiscard]] Result<handle> open(std::string path, Mode mode);

/// Close the file asynchronously.
[[nodiscard]] Result<void> close(handle file);

/// Read the file asynchronously, make sure the buffer is valid until the task is done.
[[nodiscard]] Result<ssize_t> read(handle file, char* buffer, std::size_t size);

[[nodiscard]] Result<std::string> read(std::string path, Mode mode = Mode::Read);

/// Write the file asynchronously, make sure the buffer is valid until the task is done.
[[nodiscard]] Result<void> write(handle file, char* buffer, std::size_t size);

[[nodiscard]] Result<void> write(std::string path,
                                      char* buffer,
                                      std::size_t size,
                                      Mode mode = Mode(Mode::Write, Mode::Create, Mode::Truncate));

struct Stats {
    std::chrono::milliseconds mtime;
};

Result<Stats> stat(std::string path);

}  // namespace fs

}  // namespace clice::async

