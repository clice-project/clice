#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace clice::protocol {

// reflectable struct definition
template <typename... Ts>
struct Record : Ts... {};

#define CLICE_RECORD(name, ...)                                                                                        \
    struct name##Body;                                                                                                 \
    using name = Record<__VA_ARGS__, name##Body>;                                                                      \
    struct name##Body

/// range in [-2^31, 2^31- 1]
using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string;

using DocumentUri = std::string;

struct Position {
    uinteger line;
    uinteger character;
};

struct Range {
    Position start;
    Position end;
};

struct TextDocumentItem {
    DocumentUri uri;
    string languageId;
    integer version;
    string text;
};

}  // namespace clice::protocol
