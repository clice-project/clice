#include "Support/Logger.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace clice::logging {

Options options;

void create_stderr_logger(std::string_view name, const Options& options) {
    std::shared_ptr<spdlog::logger> logger;
    if(options.color) {
        logger = spdlog::stderr_color_mt(std::string(name));
    } else {
        logger = spdlog::stderr_logger_mt(std::string(name));
    }
    logger->set_level(options.level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] %v");
    spdlog::set_default_logger(std::move(logger));
}

}  // namespace clice::logging
