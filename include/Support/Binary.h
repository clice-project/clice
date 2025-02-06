#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "Enum.h"
#include "Struct.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace clice::binary {

namespace impl {

template <typename Tuple>
struct tuple_uniuqe {
    using type = Tuple;
};

/// Uniuqe the types in the tuple.
template <typename Tuple>
using tuple_uniuqe_t = typename tuple_uniuqe<Tuple>::type;

template <typename T, typename... Ts>
    requires (!std::is_same_v<T, Ts> && ...)
struct tuple_uniuqe<std::tuple<T, Ts...>> {
    using type = decltype(std::tuple_cat(std::declval<std::tuple<T>>(),
                                         std::declval<tuple_uniuqe_t<std::tuple<Ts...>>>()));
};

template <typename T, typename... Ts>
    requires (std::is_same_v<T, Ts> || ...)
struct tuple_uniuqe<std::tuple<T, Ts...>> {
    using type = tuple_uniuqe_t<std::tuple<Ts...>>;
};

template <typename T>
struct array {
    uint32_t offset;
    uint32_t size;
};

using string = array<char>;

/// Check whether a type can be directly binarized.
template <typename T>
constexpr inline bool is_directly_binarizable_v = false;

template <typename T>
    requires (std::is_integral_v<T> || refl::reflectable_enum<T>)
constexpr inline bool is_directly_binarizable_v<T> = true;

template <refl::reflectable_struct T>
constexpr inline bool is_directly_binarizable_v<T> = refl::member_types<T>::apply(
    []<typename... Ts>() { return (is_directly_binarizable_v<Ts> && ...); });

template <typename T>
struct binarify;

/// Transform a type to its binary representation. All `std::string`
/// will be transformed to `string`. All `std::vector<T>` will be
/// transformed to `array<T>`.
template <typename T>
using binarify_t = typename binarify<T>::type;

/// For types that can be directly binarized, the binary representation
/// is the same as the original type.
template <typename T>
    requires (is_directly_binarizable_v<T>)
struct binarify<T> {
    using type = T;
};

template <>
struct binarify<std::string> {
    using type = string;
};

template <typename V>
struct binarify<std::vector<V>> {
    using type = array<V>;
};

template <typename... Ts>
struct binarify<std::tuple<Ts...>> {
    using type = std::tuple<binarify_t<Ts>...>;
};

/// For reflectable struct, transform it recursively.
template <typename T>
    requires (refl::reflectable_struct<T> && !is_directly_binarizable_v<T>)
struct binarify<T> : binarify<typename refl::member_types<T>::to_tuple> {};

/// A section in the binary data.
template <typename T>
struct Section {
    /// Current count of elements.
    uint32_t count = 0;

    /// Total count of elements in the section.
    uint32_t total = 0;

    /// Offset of the section.
    uint32_t offset = 0;
};

template <typename T>
struct layout;

/// Get the binary layout of a type. Make sure every type in the
/// layout is unique.
template <typename T>
using layout_t = tuple_uniuqe_t<typename layout<T>::type>;

template <typename T>
    requires (is_directly_binarizable_v<T>)
struct layout<T> {
    using type = std::tuple<>;
};

template <>
struct layout<std::string> {
    using type = std::tuple<Section<char>>;
};

/// Every time we encounter a `std::vector<T>`, we will add a `section<T>`.
template <typename T>
struct layout<std::vector<T>> {
    using type = decltype(std::tuple_cat(std::declval<std::tuple<Section<T>>>(),
                                         std::declval<layout_t<T>>()));
};

template <typename... Ts>
struct layout<std::tuple<Ts...>> {
    using type = decltype(std::tuple_cat(std::declval<layout_t<Ts>>()...));
};

/// For reflectable struct, recursively get the layout.
template <typename T>
    requires (refl::reflectable_struct<T> && !is_directly_binarizable_v<T>)
struct layout<T> {
    using type = layout_t<typename refl::member_types<T>::to_tuple>;
};

template <typename T>
struct Packer {
    /// The layout of the binary data.
    layout_t<T> layout = {};

    /// The total size of the binary data.
    uint32_t size = 0;

    /// The buffer to store the binary data.
    char* buffer = nullptr;

    /// Recursively traverse the object and calculate the size of each section.
    template <typename Object>
    void init(const Object& object) {
        if constexpr(std::same_as<Object, std::string>) {
            std::get<Section<char>>(layout).total += object.size() + 1;
        } else if constexpr(requires { typename Object::value_type; }) {
            std::get<Section<typename Object::value_type>>(layout).total += object.size();
            for(const auto& element: object) {
                init(element);
            }
        } else if constexpr(refl::reflectable_struct<Object>) {
            refl::foreach(object, [&](auto, auto& field) { init(field); });
        }
    }

    /// Write the object to the buffer and return the binary representation.
    template <typename Object>
    auto write(const Object& object) {
        if constexpr(is_directly_binarizable_v<Object> && !refl::reflectable_struct<Object>) {
            return object;
        } else if constexpr(std::same_as<Object, std::string>) {
            auto& section = std::get<Section<char>>(layout);
            uint32_t size = object.size();
            uint32_t offset = section.offset + section.count;
            section.count += size + 1;

            std::memcpy(buffer + offset, object.data(), size);
            buffer[offset + size] = '\0';

            return string{offset, size};
        } else if constexpr(requires { typename Object::value_type; }) {
            using V = typename Object::value_type;
            auto& section = std::get<Section<V>>(layout);
            uint32_t size = object.size();
            uint32_t offset = section.offset + section.count * sizeof(binarify_t<V>);
            section.count += size;

            for(std::size_t i = 0; i < size; ++i) {
                ::new (buffer + offset + i * sizeof(binarify_t<V>)) auto{write(object[i])};
            }

            return array<V>{offset, size};
        } else if constexpr(refl::reflectable_struct<Object>) {
            std::array<char, sizeof(binarify_t<Object>)> buffer;
            std::memset(buffer.data(), 0, sizeof(buffer));

            binarify_t<Object> result;
            refl::foreach(result, object, [&](auto& lhs, auto& rhs) {
                auto offset = reinterpret_cast<char*>(&lhs) - reinterpret_cast<char*>(&result);
                ::new (buffer.data() + offset) auto{write(rhs)};
            });

            return buffer;
        } else {
            static_assert(dependent_false<Object>, "Unsupported type.");
        }
    }

    char* pack(const auto& object) {
        /// First initialize the layout.
        init(object);

        /// Calculate the total size of the binary data and
        /// the offset of each section.
        size = sizeof(binarify_t<T>);

        auto try_each = [&]<typename V>(auto, Section<V>& field) {
            static_assert(alignof(binarify_t<V>) <= 8, "Alignment not supported.");

            /// Make sure each section is aligned to 8 bytes.
            if(size % 8 != 0) {
                size += 8 - size % 8;
            }

            field.offset = size;
            size += field.total * sizeof(binarify_t<V>);
        };

        refl::foreach(layout, try_each);

        /// Allocate the buffer and write the data to the buffer.
        buffer = static_cast<char*>(std::malloc(size));

        /// Make sure the buffer is clean. So we can compare the result.
        /// Every padding in the struct should be filled with 0.
        std::memset(buffer, 0, size);

        /// Write the object to the buffer.
        auto result = write(object);
        std::memcpy(buffer, &result, sizeof(result));

        return buffer;
    }
};

}  // namespace impl

template <std::size_t N>
struct fixed_string : std::array<char, N + 1> {
    template <std::size_t M>
    constexpr fixed_string(const char (&str)[M]) {
        for(std::size_t i = 0; i < N; ++i) {
            this->data()[i] = str[i];
        }
        this->data()[N] = '\0';
    }

    constexpr auto size() const {
        return N;
    }

    constexpr operator std::string_view () const {
        return {this->data(), N};
    }
};

template <std::size_t M>
fixed_string(const char (&)[M]) -> fixed_string<M - 1>;

/// A helper class to access the binary data.
template <typename T>
struct Proxy {
    using underlying_type = impl::binarify_t<T>;
    const void* base;
    const void* data;

    const auto& value() const {
        return *reinterpret_cast<const underlying_type*>(data);
    }

    template <std::size_t I>
    auto get() const {
        return Proxy<refl::member_type<T, I>>{base, &std::get<I>(value())};
    }

    template <fixed_string name>
    auto get() const {
        constexpr auto& names = refl::member_names<T>();

        constexpr auto index = []() {
            for(std::size_t i = 0; i < names.size(); ++i) {
                if(names[i] == name) {
                    return i;
                }
            }
            return names.size();
        }();

        return this->template get<index>();
    }

    auto as_string() const {
        auto [offset, size] = value();
        return llvm::StringRef{reinterpret_cast<const char*>(base) + offset, size};
    }

    auto as_array() const {
        auto [offset, size] = value();
        using U = impl::binarify_t<typename T::value_type>;
        return llvm::ArrayRef<U>{
            reinterpret_cast<const U*>(static_cast<const char*>(base) + offset),
            size,
        };
    }

    auto operator[] (std::size_t index) const {
        return Proxy<typename T::value_type>{base, &as_array()[index]};
    }

    auto size() const {
        return value().size;
    }

    auto operator->() const {
        return &value();
    }

    operator const underlying_type& () const {
        return value();
    }
};

/// Binirize an object.
template <typename Object>
std::pair<Proxy<Object>, size_t> binarify(const Object& object) {
    impl::Packer<Object> packer;
    auto buffer = packer.pack(object);
    return {
        Proxy<Object>{buffer, reinterpret_cast<const impl::binarify_t<Object>*>(buffer)},
        packer.size
    };
}

}  // namespace clice::binary
