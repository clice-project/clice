#include <spdlog/spdlog.h>

namespace clice::logger {

extern void* instance;

void init(std::string_view path);

inline void info(std::string_view message) {
    spdlog::info(message);
    spdlog::default_logger()->flush();
}

template <typename... Args>
inline void info(spdlog::format_string_t<Args...> message, Args&&... args) {
    spdlog::info(message, std::forward<Args>(args)...);
    spdlog::default_logger()->flush();
}

inline void error(std::string_view message) {
    spdlog::error(message);
    spdlog::default_logger()->flush();
}

template <typename... Args>
inline void error(spdlog::format_string_t<Args...> message, Args&&... args) {
    spdlog::error(message, std::forward<Args>(args)...);
    spdlog::default_logger()->flush();
}

}  // namespace clice::logger
