#pragma once

#include <gtest/gtest.h>

#include <Support/FileSystem.h>
#include <spdlog/fmt/bundled/color.h>
#include <llvm/Support/MemoryBuffer.h>
#include <source_location>

namespace clice {
std::string test_dir();

template <typename Callback>
inline void foreachFile(std::string name, const Callback& callback) {
    llvm::SmallString<128> path;
    path += test_dir();
    path::append(path, name);
    std::error_code error;
    fs::directory_iterator iter(path, error);
    fs::directory_iterator end;
    while(!error && iter != end) {
        auto file = iter->path();
        auto buffer = llvm::MemoryBuffer::getFile(file);
        if(!buffer) {
            llvm::outs() << "failed to open file: " << buffer.getError().message() << file << "\n";
            // TODO:
        }
        auto content = buffer.get()->getBuffer();
        callback(file, content);
        iter.increment(error);
    }
}

}  // namespace clice

