#pragma once

#include <tuple>
#include <array>
#include <string>
#include <cstdint>
#include <string_view>
#include <source_location>

#include "TypeTraits.h"

namespace clice::support {

template <typename T>
    requires std::is_enum_v<T>
constexpr auto underlying_value(T value) {
    return static_cast<std::underlying_type_t<T>>(value);
}

template <auto value>
    requires std::is_enum_v<decltype(value)>
consteval auto enum_name() {
    std::string_view name = std::source_location::current().function_name();
#if __GNUC__ || __clang__
    std::size_t start = name.find('=') + 2;
    std::size_t end = name.size() - 1;
#elif _MSC_VER
    std::size_t start = name.find('<') + 1;
    std::size_t end = name.rfind(">(");
#else
    static_assert(false, "Not supported compiler");
#endif
    name = name.substr(start, end - start);
    start = name.rfind("::");
    return start == std::string_view::npos ? name : name.substr(start + 2);
}

template <typename E, std::size_t N = 0>
consteval auto enum_max() {
    constexpr auto value = std::bit_cast<E>(static_cast<std::underlying_type_t<E>>(N));
    if constexpr(enum_name<value>().find(")") == std::string_view::npos)
        return enum_max<E, N + 1>();
    else
        return N;
}

template <typename E, std::size_t count>
struct enum_table {
    constexpr static std::array<std::string_view, count> table =
        []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array{enum_name<static_cast<E>(Is)>()...};
        }(std::make_index_sequence<count>{});
};

template <typename E, std::size_t begin = 0, std::size_t end = enum_max<E, begin>()>
constexpr std::string_view enum_name(E value) {
    return enum_table<E, end - begin>::table[static_cast<std::underlying_type_t<E>>(value) - begin];
}

/// A helper class to define enum.
template <typename Derived, bool is_bitmask = false, typename underlying = uint8_t>
class Enum {
public:
    /// Tag to indicate this is a special enum.
    constexpr inline static bool is_special_enum_v = true;

    constexpr Enum() : m_Value(invalid()) {}

    /// A integral must explicitly convert to the enum.
    explicit constexpr Enum(underlying value) : m_Value(value) {}

    /// Allow the enum to be constructed from the enum value.
    template <typename Kind>
        requires std::is_same_v<Kind, typename Derived::Kind>
    constexpr Enum(Kind kind) : m_Value(kind) {
        static_assert(sizeof(underlying) >= sizeof(typename Derived::Kind),
                      "Underlying type is too small to hold all enum values.");
    }

    constexpr Enum(const Enum&) = default;

    constexpr Enum& operator= (const Enum&) = default;

    /// Get the underlying value of the enum.
    constexpr underlying value() const {
        return m_Value;
    }

    /// Get the name of the enum.
    constexpr std::string_view name() const {
        using E = typename Derived::Kind;
        return support::enum_name<E, begin(), end()>(static_cast<E>(m_Value));
    }

    constexpr explicit operator bool () const {
        return m_Value != invalid();
    }

    constexpr friend bool operator== (Enum lhs, Enum rhs) = default;

    constexpr static auto& all() {
        return enum_table<typename Derived::Kind, end() - begin()>::table;
    }

private:
    consteval static underlying begin() {
        if constexpr(requires { Derived::FirstEnum; }) {
            return Derived::FirstEnum;
        } else {
            return 0;
        }
    }

    consteval static underlying end() {
        if constexpr(requires { Derived::LastEnum; }) {
            return Derived::LastEnum;
        } else {
            return support::enum_max<typename Derived::Kind, begin()>();
        }
    }

    consteval static underlying invalid() {
        if constexpr(requires { Derived::InvalidEnum; }) {
            return Derived::InvalidEnum;
        } else {
            static_assert(dependent_false<Derived>, "Invalid enum value is not defined.");
        }
    }

private:
    underlying m_Value;
};

template <typename Derived, typename underlying>
class Enum<Derived, true, underlying> {
public:
    /// Tag to indicate this is a special enum.
    constexpr inline static bool is_special_enum_v = true;

    /// FIXME:
    Enum() = default;

    /// A integral must explicitly convert to the enum.
    explicit constexpr Enum(underlying value) : m_Value(value) {}

    /// Allow the enum to be constructed from the enum value.
    template <typename... Kinds>
        requires (std::is_same_v<Kinds, typename Derived::Kind> && ...)
    constexpr Enum(Kinds... kind) : m_Value(((1 << underlying_value(kind)) | ...)) {
        static_assert(sizeof(underlying) * 8 >= end(),
                      "Underlying type is too small to hold all enum values.");
    }

    constexpr Enum(const Enum&) = default;

    constexpr Enum& operator= (const Enum&) = default;

    /// Get the underlying value of the enum.
    constexpr underlying value() const {
        return m_Value;
    }

    /// Get the name of the enum.
    constexpr std::string name() const {
        std::string masks;
        bool isFirst = true;
        for(std::size_t i = 0; i < sizeof(underlying) * 8; i++) {
            bool hasBit = m_Value & (1 << i);

            if(!hasBit) {
                continue;
            }

            if(isFirst) {
                isFirst = false;
            } else {
                masks += " | ";
            }

            using E = typename Derived::Kind;
            masks += support::enum_name<E, begin(), end()>(static_cast<E>(i));
        }
        return masks;
    }

    constexpr static auto& all() {
        return enum_table<typename Derived::Kind, end() - begin()>::table;
    }

    constexpr explicit operator bool () const {
        return m_Value != 0;
    }

    constexpr friend bool operator== (Enum lhs, Enum rhs) = default;

    template <typename Kind>
        requires std::is_same_v<Kind, typename Derived::Kind>
    constexpr Enum operator| (Kind kind) const {
        return Enum(m_Value | (1 << underlying_value(kind)));
    }

    template <typename Kind>
        requires std::is_same_v<Kind, typename Derived::Kind>
    constexpr Enum operator& (Kind kind) const {
        return Enum(m_Value & (1 << underlying_value(kind)));
    }

    template <typename Kind>
        requires std::is_same_v<Kind, typename Derived::Kind>
    constexpr Enum& operator|= (Kind kind) {
        m_Value |= (1 << underlying_value(kind));
        return *this;
    }

    template <typename Kind>
        requires std::is_same_v<Kind, typename Derived::Kind>
    constexpr Enum& operator&= (Kind kind) {
        m_Value &= (1 << underlying_value(kind));
        return *this;
    }

private:
    consteval static std::size_t begin() {
        if constexpr(requires { Derived::FirstEnum; }) {
            return Derived::FirstEnum;
        } else {
            return 0;
        }
    }

    consteval static std::size_t end() {
        if constexpr(requires { Derived::LastEnum; }) {
            return Derived::LastEnum;
        } else {
            return support::enum_max<typename Derived::Kind, begin()>();
        }
    }

private:
    underlying m_Value;
};

template <typename T>
concept special_enum = requires {
    T::is_special_enum_v;
    requires T::is_special_enum_v;
};

}  // namespace clice::support

