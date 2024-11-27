#pragma once

#include <Support/Format.h>
#include <Support/FileSystem.h>
#include <llvm/ADT/SmallString.h>

namespace clice::log {

enum class Level {
    INFO,
    ERROR,
    WARN,
    DEBUG,
    TRACE,
};

template <typename... Args>
void log(Level level, std::string_view fmt, Args&&... args) {
    namespace chrono = std::chrono;
    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    auto time = chrono::zoned_time(chrono::current_zone(), now);
    auto tag = [&] {
        switch(level) {
            case Level::INFO: return "\033[32mINFO\033[0m";    // Green
            case Level::ERROR: return "\033[31mERROR\033[0m";  // Red
            case Level::WARN: return "\033[33mWARN\033[0m";    // Yellow
            case Level::DEBUG: return "\033[36mDEBUG\033[0m";  // Cyan
            case Level::TRACE: return "\033[35mTRACE\033[0m";  // Magenta
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
void error(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::ERROR, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::WARN, fmt.get(), std::forward<Args>(args)...);
}

}  // namespace clice::log
