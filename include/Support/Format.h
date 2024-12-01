#pragma once

#include <format>

#include "llvm/ADT/StringRef.h"
#include "Error.h"
#include "JSON.h"

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

