#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Support/FileSystem.h>
#include <llvm/ADT/SmallString.h>

namespace clice::logger {

inline auto init(std::string_view type, std::string_view filepath) {
    if(type == "file") {
        llvm::SmallString<128> temp;
        temp.append(path::parent_path(path::parent_path(filepath)));
        path::append(temp, "logs");

        auto error = llvm::sys::fs::make_absolute(temp);
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_c);

        std::ostringstream timeStream;
        timeStream << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S");

        std::string logFileName = "clice_" + timeStream.str() + ".log";
        path::append(temp, logFileName);

        auto logger = spdlog::basic_logger_mt("clice", temp.c_str());
        logger->flush_on(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    } else if(type == "console") {
        auto logger = spdlog::stdout_color_mt("clice");
        logger->flush_on(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    }
}

using spdlog::info;
using spdlog::error;
using spdlog::warn;
using spdlog::debug;
using spdlog::trace;

}  // namespace clice::logger
