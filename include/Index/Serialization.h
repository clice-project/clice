#pragma once

#include <Index/CSIF.h>

namespace clice {

class MemorySlab {
    constexpr inline static std::size_t defaultSize = 4096;
    constexpr inline static std::size_t ratio = 8;

public:
    MemorySlab() = default;

public:
    std::size_t allocate(std::uint32_t number) {
        char* ptr = alloc(sizeof(std::uint32_t));
        *reinterpret_cast<std::uint32_t*>(ptr) = number;
        return ptr - data;
    }

    std::size_t allocate(llvm::StringRef str) {
        char* ptr = alloc(str.size() + 1);
        std::memcpy(ptr, str.data(), str.size());
        ptr[str.size()] = '\0';
        return ptr - data;
    }

    template <typename T>
    std::size_t allocate(ArrayRef<T> array) {
        // TODO:
    }

private:
    void grow(std::size_t capacity) {
        auto newCapacity = capacity * ratio;
        if(data == nullptr) {
            data = static_cast<char*>(std::malloc(newCapacity));
        } else {
            data = static_cast<char*>(std::realloc(data, newCapacity));
        }
        end = data + capacity;
        max = data + newCapacity;
    }

    char* alloc(std::size_t size) {
        if(end + size > max) {
            grow(max - data);
        }
        auto ptr = end;
        end += size;
        return ptr;
    }

private:
    char* data = nullptr;
    char* end = nullptr;
    char* max = nullptr;
};

}  // namespace clice
