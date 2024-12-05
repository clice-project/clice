#pragma once

#include <chrono>

namespace clice {

struct Tracer {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

    auto duration() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start);
    }
};

}  // namespace clice
