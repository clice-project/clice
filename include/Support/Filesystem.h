#pragma once

#include <cstdio>
#include <filesystem>

namespace clice {

namespace fs = std::filesystem;

inline std::string readAll(std::string_view path) {
    std::string result;
    FILE* file = fopen(path.data(), "r");
    if(file) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        result.resize(size);
        fseek(file, 0, SEEK_SET);
        fread(result.data(), 1, size, file);
        fclose(file);
    }
    return result;
}

}  // namespace clice
