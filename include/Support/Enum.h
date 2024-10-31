#pragma once

#include <format>
#include <llvm/Support/raw_ostream.h>

#include <Support/Reflection.h>

namespace clice {

template <typename T>
    requires std::is_enum_v<T>
auto underlying_value(T value) {
    return static_cast<std::underlying_type_t<T>>(value);
}

template <typename Enum, bool is_mask = false>
    requires std::is_enum_v<Enum>
struct enum_type {
    using enum_tag = Enum;

    constexpr explicit enum_type() = default;

    constexpr explicit enum_type(std::underlying_type_t<Enum> value) : value(value) {}

    constexpr enum_type(Enum enum_) : value(underlying_value(enum_)) {}

    friend constexpr bool operator== (enum_type lhs, enum_type rhs) = default;

    friend llvm::raw_ostream& operator<< (llvm::raw_ostream& os, enum_type e) {
        os << std::format("[normal enum, value: {}, name: {}]",
                          e.value,
                          refl::enum_name<Enum>(static_cast<Enum>(e.value)));
        return os;
    }

    std::underlying_type_t<Enum> value;
};

template <typename Enum>
    requires std::is_enum_v<Enum>
struct enum_type<Enum, true> {
public:
    using enum_tag = Enum;

    constexpr explicit enum_type() = default;

    template <typename... Args>
    constexpr explicit enum_type(Args... args) : value(((1 << underlying_value(args)) | ...)) {}

    constexpr explicit enum_type(std::underlying_type_t<Enum> value) : value(value) {}

    constexpr enum_type(Enum enum_) : value(1 << underlying_value(enum_)) {}

    constexpr void set(Enum enum_) {
        value |= (1 << underlying_value(enum_));
    }

    constexpr bool is(Enum enum_) const {
        return value & (1 << underlying_value(enum_));
    }

    friend constexpr bool operator== (enum_type lhs, enum_type rhs) = default;

    friend llvm::raw_ostream& operator<< (llvm::raw_ostream& os, enum_type e) {
        std::string masks;
        bool is_first = true;
        for(std::size_t i = 0; i < sizeof(std::underlying_type_t<Enum>) * 8; i++) {
            if(e.value & (1 << i)) {
                if(is_first) {
                    is_first = false;
                } else {
                    masks += " | ";
                }
                masks += refl::enum_name<Enum>(static_cast<Enum>(i));
            }
        }

        os << std::format("[mask enum, value: {}, masks: {}]", e.value, masks);
        return os;
    }

    std::underlying_type_t<Enum> value;
};

template <typename T>
    requires requires { typename T::enum_tag; }
auto underlying_value(T value) {
    return value.value;
}

template <typename T>
constexpr inline bool is_enum_v = std::is_enum_v<T> || requires { typename T::enum_tag; };

}  // namespace clice
