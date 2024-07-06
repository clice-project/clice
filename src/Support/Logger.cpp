#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <Support/Filesystem.h>

namespace clice::logger {

void* instance = nullptr;

void init(std::string_view path) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = std::localtime(&time);
    std::string filename = fmt::format("{}-{:02d}-{:02d}--{:02d}-{:02d}-{:02d}.log",
                                       tm->tm_year + 1900,
                                       tm->tm_mon + 1,
                                       tm->tm_mday,
                                       tm->tm_hour,
                                       tm->tm_min,
                                       tm->tm_sec);
    auto dir = fs::path(path).parent_path() / "logs";

    if(!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    instance = new auto(spdlog::basic_logger_mt("clice", (dir / filename).string()));
    auto& logger = *static_cast<std::shared_ptr<spdlog::logger>*>(instance);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
}

}  // namespace clice::logger
