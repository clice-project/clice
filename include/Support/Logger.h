#pragma once

#include "Format.h"
#include "FileSystem.h"

#include "llvm/Support/CommandLine.h"

namespace clice::log {

enum class Level {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    FATAL,
};

struct LogOpt {
    Level level = Level::INFO;
    bool color = false;
};

inline LogOpt log_opt;

template <typename... Args>
void log(Level level, std::string_view fmt, Args&&... args) {
#define Green "\033[32m"
#define Yellow "\033[33m"
#define Cyan "\033[36m"
#define Magenta "\033[35m"
#define Red "\033[31m"
#define None "\033[0m"
    namespace chrono = std::chrono;
    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    auto time = now;  // chrono::zoned_time(chrono::current_zone(), now);
    auto tag = [&] {
        switch(level) {
            case Level::INFO: return log_opt.color ? Green "[INFO]" None : "[INFO]";    // Green
            case Level::WARN: return log_opt.color ? Yellow "[WARN]" None : "[WARN]";   // Yellow
            case Level::DEBUG: return log_opt.color ? Cyan "[DEBUG]" None : "[DEBUG]";  // Cyan
            case Level::TRACE:
                return log_opt.color ? Magenta "[TRACE]" None : "[TRACE]";  // Magenta
            case Level::FATAL:
                return log_opt.color ? Red "[FATAL ERROR]" None : "[FATAL ERROR]";  // Red
            default: llvm::llvm_unreachable_internal("Illegal log level");
        }
    }();
    if(level >= log_opt.level) {
        llvm::errs() << std::format("[{0:%Y-%m-%d %H:%M:%S}] {1} ", time, tag)
                     << std::vformat(fmt, std::make_format_args(args...)) << "\n";
    }
#undef Green
#undef Yellow
#undef Cyan
#undef Magenta
#undef Red
#undef None
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
#ifndef NDEBUG
    log::log(Level::DEBUG, fmt.get(), std::forward<Args>(args)...);
#endif
}

template <typename... Args>
void trace(std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::TRACE, fmt.get(), std::forward<Args>(args)...);
}

template <typename... Args>
void fatal [[noreturn]] (std::format_string<Args...> fmt, Args&&... args) {
    log::log(Level::FATAL, fmt.get(), std::forward<Args>(args)...);
    std::abort();
}

}  // namespace clice::log
