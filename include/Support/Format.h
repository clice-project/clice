#pragma once

#include <format>

#include "llvm/ADT/StringRef.h"

template <>
struct std::formatter<llvm::StringRef> : std::formatter<std::string_view> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return std::formatter<std::string_view>::parse(ctx);
    }

    template <typename FormatContext>
    auto format(llvm::StringRef s, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(std::string_view(s.str()), ctx);
    }
};
