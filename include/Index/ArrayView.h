#pragma once

#include <cstdint>
#include <iterator>

namespace clice::index {

/// When serialize index to binary, we will transform all pointer to offset
/// to base address. And data only will be deserialized when it is accessed.
struct Relative {
    const void* base;
    const void* data;

    bool operator== (const Relative& other) const = default;
};

template <typename T>
class ArrayView : Relative {
public:
    ArrayView(const void* base, const void* data, std::size_t size, std::size_t stride) :
        Relative{base, data}, size(size), stride(stride) {}

    class Iterator : Relative {
    public:
        Iterator(const void* base, const void* data, std::size_t stride) :
            Relative{base, data}, stride(stride) {}

        T operator* () const {
            return T{base, data};
        }

        Iterator& operator++ () {
            data = static_cast<const char*>(data) + stride;
            return *this;
        }

        bool operator== (const Iterator& other) const = default;

    private:
        std::size_t stride;
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

    T operator[] (uint32_t index) const {
        return T{base, static_cast<const char*>(data) + index * stride};
    }

    bool operator== (const ArrayView& other) const = default;

private:
    std::size_t size;
    std::size_t stride;
};

}  // namespace clice::index
