#pragma once

#include <Support/Format.h>
#include <Support/FileSystem.h>
#include <source_location>

namespace clice::log {

enum class Level {
    INFO,
    WARN,
    DEBUG,
    TRACE,
    FATAL,
};

template <typename... Args>
struct FmtStrWithLoc {
    std::string_view format;
    std::source_location location;

    constexpr FmtStrWithLoc(const char* fmt,
                            std::source_location loc = std::source_location::current()) :
        format(fmt), location(loc) {
        // check if the format string is valid in compile-time
        // this will give a readable error message
        [[maybe_unused]] bool is_invalid_format = (std::format_string<Args...>{fmt}, true);
    }
};

/// As the notes in https://en.cppreference.com/w/cpp/utility/format/basic_format_string.
/// The alias templates `Fswl` use std::type_identity_t to inhibit
/// template argument deduction. Typically, when they appear as a function parameter, their template
/// arguments are deduced from other function arguments.
template <typename... Args>
using Fswl = FmtStrWithLoc<std::type_identity_t<Args>...>;

template <typename... Args>
void log(Level level, std::string_view fmt, std::source_location _loc, Args&&... args) {
    namespace chrono = std::chrono;
    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    auto time = chrono::zoned_time(chrono::current_zone(), now);
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
void info(Fswl<Args...> fmt, Args&&... args) {
    log::log(Level::INFO, fmt.format, fmt.location, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(Fswl<Args...> fmt, Args&&... args) {
    log::log(Level::WARN, fmt.format, fmt.location, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(Fswl<Args...> fmt, Args&&... args) {
    log::log(Level::DEBUG, fmt.format, fmt.location, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(Fswl<Args...> fmt, Args&&... args) {
    log::log(Level::TRACE, fmt.format, fmt.location, std::forward<Args>(args)...);
}

template <typename... Args>
void fatal [[noreturn]] (Fswl<Args...> fmt, Args&&... args) {
    log::log(Level::FATAL, fmt.format, fmt.location, std::forward<Args>(args)...);
    std::terminate();
}

}  // namespace clice::log
