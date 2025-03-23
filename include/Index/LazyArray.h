#pragma once

#include <cstdint>
#include <iterator>
#include <ranges>
#include <algorithm>

namespace clice::index {

/// When serialize index to binary, we will transform all pointer to offset
/// to base address. And data only will be deserialized when it is accessed.
struct Relative {
    const void* base = nullptr;
    const void* data = nullptr;

    bool operator== (const Relative& other) const = default;
};

template <typename T>
class LazyArray : Relative {
public:
    LazyArray(const void* base, const void* data, std::uint32_t size, std::uint32_t stride) :
        Relative{base, data}, size(size), stride(stride) {}

    using difference_type = std::ptrdiff_t;
    using value_type = T;

    class Iterator : Relative {
    public:
        Iterator(const void* base, const void* data, std::size_t stride) :
            Relative{base, data}, stride(stride) {}

        using difference_type = std::ptrdiff_t;
        using value_type = T;

        Iterator() = default;

        Iterator(const Iterator&) = default;

        decltype(auto) operator* () const {
            if constexpr(std::derived_from<T, Relative>) {
                return T{base, data};
            } else {
                return *static_cast<const T*>(data);
            }
        }

        Iterator& operator++ () {
            data = static_cast<const char*>(data) + stride;
            return *this;
        }

        Iterator operator++ (int) {
            ++*this;
        }

        bool operator== (const Iterator& other) const = default;

    private:
        std::uint32_t stride;
    };

    Iterator begin() const {
        return Iterator(base, data, stride);
    }

    Iterator end() const {
        return Iterator(base, static_cast<const char*>(data) + size * stride, stride);
    }

    uint32_t length() const {
        return size;
    }

    decltype(auto) operator[] (uint32_t index) const {
        const void* data = static_cast<const char*>(this->data) + index * stride;
        if constexpr(std::derived_from<T, Relative>) {
            return T{base, data};
        } else {
            return *static_cast<const T*>(data);
        }
    }

    bool operator== (const LazyArray& other) const = default;

private:
    std::uint32_t size;
    std::uint32_t stride;
};

}  // namespace clice::index
