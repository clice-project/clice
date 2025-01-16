#pragma once

#include <format>
#include <ranges>

#include "Error.h"
#include "JSON.h"

namespace clice {

template <typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    llvm::outs() << std::vformat(fmt.get(), std::make_format_args(args...));
}

template <typename... Ts>
constexpr inline bool is_formattable_v = false;

template <typename T, typename... Args>
constexpr inline bool is_formattable_v<T, Args...> =
    std::is_convertible_v<T, std::format_string<Args...>>;

template <typename... Args>
    requires (!is_formattable_v<Args...>)
void print(Args&&... args) {
    ((llvm::outs() << std::format("{}", std::make_format_args(args)) << " "), ...);
}

template <typename... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    llvm::outs() << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

template <typename... Args>
    requires (!is_formattable_v<Args...>)
void println(Args&&... args) {
    ((llvm::outs() << std::format("{}", std::make_format_args(args)) << " "), ...) << '\n';
}

}  // namespace clice

template <>
struct std::formatter<llvm::StringRef> : std::formatter<std::string_view> {
    using Base = std::formatter<std::string_view>;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return Base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(llvm::StringRef s, FormatContext& ctx) const {
        return Base::format(std::string_view(s.str()), ctx);
    }
};

template <>
struct std::formatter<llvm::Error> : std::formatter<llvm::StringRef> {
    using Base = std::formatter<llvm::StringRef>;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return Base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(const llvm::Error& e, FormatContext& ctx) const {
        llvm::SmallString<128> buffer;
        llvm::raw_svector_ostream os(buffer);
        os << e;
        return Base::format(buffer, ctx);
    }
};

template <std::size_t N>
struct std::formatter<llvm::SmallString<N>> : std::formatter<llvm::StringRef> {
    using Base = std::formatter<llvm::StringRef>;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return Base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(const llvm::SmallString<N>& s, FormatContext& ctx) const {
        return Base::format(llvm::StringRef(s), ctx);
    }
};

template <>
struct std::formatter<clice::json::Value> : std::formatter<llvm::StringRef> {
    using Base = std::formatter<llvm::StringRef>;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return Base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(const clice::json::Value& value, FormatContext& ctx) const {
        llvm::SmallString<128> buffer;
        llvm::raw_svector_ostream os{buffer};
        os << value;
        return Base::format(buffer, ctx);
    }
};

template <clice::refl::reflectable_enum E>
struct std::formatter<E> : std::formatter<std::string_view> {
    using Base = std::formatter<std::string_view>;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return Base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(const E& e, FormatContext& ctx) const {
        return Base::format(e.name(), ctx);
    }
};

namespace clice {}  // namespace clice
