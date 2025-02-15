#pragma once

#include "Format.h"
#include "FileSystem.h"

namespace clice::log {

enum class Level {
    INFO,
    WARN,
    DEBUG,
    TRACE,
    FATAL,
};

template <typename... Args>
void log(Level level, std::string_view fmt, Args&&... args) {
    namespace chrono = std::chrono;
    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    auto time = now;  // chrono::zoned_time(chrono::current_zone(), now);
    auto tag = [&] {
        switch(level) {
            case Level::INFO: return "\033[32mINFO\033[0m";          // Green
            case Level::WARN: return "\033[33mWARN\033[0m";          // Yellow
            case Level::DEBUG: return "\033[36mDEBUG\033[0m";        // Cyan
            case Level::TRACE: return "\033[35mTRACE\033[0m";        // Magenta
            case Level::FATAL: return "\033[31mFATAL ERROR\033[0m";  // Red
        }
    }();
    llvm::errs() << std::format("[{0:%Y-%m-%d %H:%M:%S}] [{1}] ", time, tag)
                 << std::vformat(fmt, std::make_format_args(args...)) << "\n";
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::INFO, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::WARN, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::DEBUG, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void trace(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::TRACE, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void fatal [[noreturn]] (std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::FATAL, fmt.get(), std::forward<Args>(args)...);
    std::terminate();
}

}  // namespace clice::log
