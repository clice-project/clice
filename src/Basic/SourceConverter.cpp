#include "Basic/SourceConverter.h"
#include "Basic/Location.h"

namespace clice {

/// @brief Iterates over Unicode codepoints in a UTF-8 encoded string and invokes a callback for
/// each codepoint.
///
/// Processes the input UTF-8 string, calculating the length of each Unicode codepoint in both
/// UTF-8 (bytes) and UTF-16 (code units), and passes these lengths to the callback.
/// Iteration stops early if the callback returns `false`.
///
/// ASCII characters are treated as 1-byte UTF-8 codepoints with a UTF-16 length of 1.
/// Non-ASCII characters are processed based on their leading byte to determine UTF-8 length:
/// - Valid lengths are 2 to 4 bytes.
/// - Astral codepoints (UTF-8 length of 4) have a UTF-16 length of 2 code units.
/// Invalid UTF-8 sequences are treated as single-byte ASCII characters.
///
/// Returns `false` if the callback stops the iteration.
template <typename Callback>
static bool iterateCodepoints(llvm::StringRef content, const Callback& callback) {
    // Iterate over the input string, processing each codepoint.
    for(size_t index = 0; index < content.size();) {
        unsigned char c = static_cast<unsigned char>(content[index]);

        // Handle ASCII characters (1-byte UTF-8, 1-code-unit UTF-16).
        if(!(c & 0x80)) [[likely]] {
            if(!callback(1, 1)) {
                return true;
            }

            ++index;
            continue;
        }

        // Determine the length of the codepoint in UTF-8 by counting the leading 1s.
        size_t length = llvm::countl_one(c);

        // Validate UTF-8 encoding: length must be between 2 and 4.
        if(length < 2 || length > 4) [[unlikely]] {
            assert(false && "Invalid UTF-8 sequence");

            // Treat the byte as an ASCII character.
            if(!callback(1, 1)) {
                return true;
            }

            ++index;
            continue;
        }

        // Advance the index by the length of the current UTF-8 codepoint.
        index += length;

        // Calculate the UTF-16 length: astral codepoints (4-byte UTF-8) take 2 code units.
        if(!callback(length, length == 4 ? 2 : 1)) {
            return true;
        }
    }

    return false;
}

std::size_t SourceConverter::remeasure(llvm::StringRef content, proto::PositionEncodingKind kind) {
    if(kind == proto::PositionEncodingKind::UTF8) {
        return content.size();
    }

    if(kind == proto::PositionEncodingKind::UTF16) {
        std::size_t length = 0;
        iterateCodepoints(content, [&](size_t, size_t utf16Length) {
            length += utf16Length;
            return true;
        });
        return length;
    }

    if(kind == proto::PositionEncodingKind::UTF32) {
        std::size_t length = 0;
        iterateCodepoints(content, [&](size_t, size_t) {
            length += 1;
            return true;
        });
        return length;
    }

    std::unreachable();
}

proto::Position SourceConverter::toPosition(llvm::StringRef content, clang::SourceLocation location,
                                            const clang::SourceManager& SM) const {
    assert(location.isValid() && location.isFileID() &&
           "SourceLocation must be valid and not a macro location");
    auto [fileID, offset] = SM.getDecomposedSpellingLoc(location);

    /// Line and column in LSP are 0-based but clang's SourceLocation is 1-based.
    auto line = SM.getLineNumber(fileID, offset) - 1;
    auto column = SM.getColumnNumber(fileID, offset) - 1;

    proto::Position position;
    /// Line doesn't need to be adjusted. It is encoding-dependent.
    position.line = line;

    /// Column needs to be adjusted based on the encoding.
    if(auto word = content.substr(offset - column, column); !word.empty())
        position.character = remeasure(word);
    else
        position.character = column;  // word is the last column of that line.
    return position;
}

proto::Position SourceConverter::toPosition(clang::SourceLocation location,
                                            const clang::SourceManager& SM) const {
    bool isInvalid = false;
    llvm::StringRef content = SM.getCharacterData(location, &isInvalid);
    assert(!isInvalid && "Invalid SourceLocation");
    return toPosition(content, location, SM);
}

std::size_t SourceConverter::toOffset(llvm::StringRef content, proto::Position position) const {
    std::size_t offset = 0;
    for(auto i = 0; i < position.line; i++) {
        auto pos = content.find('\n');
        assert(pos != llvm::StringRef::npos && "Line value is out of range");

        offset += pos + 1;
        content = content.substr(pos + 1);
    }

    /// Drop the content after the line.
    content = content.take_until([](char c) { return c == '\n'; });
    assert(position.character <= content.size() && "Character value is out of range");

    if(kind == proto::PositionEncodingKind::UTF8) {
        offset += position.character;
        return offset;
    }

    if(kind == proto::PositionEncodingKind::UTF16) {
        iterateCodepoints(content, [&](size_t utf8Length, size_t utf16Length) {
            assert(position.character >= utf16Length && "Character value is out of range");
            position.character -= utf16Length;
            offset += utf8Length;
            return position.character != 0;
        });
        return offset;
    }

    if(kind == proto::PositionEncodingKind::UTF32) {
        iterateCodepoints(content, [&](size_t utf8Length, size_t) {
            assert(position.character >= 1 && "Character value is out of range");
            position.character -= 1;
            offset += utf8Length;
            return position.character != 0;
        });
        return offset;
    }

    std::unreachable();
}

namespace {

/// returns true if the scheme is valid according to RFC 3986.
bool isValidScheme(llvm::StringRef scheme) {
    if(scheme.empty()) {
        return false;
    }

    if(!llvm::isAlpha(scheme[0])) {
        return false;
    }

    return llvm::all_of(llvm::drop_begin(scheme), [](char C) {
        return llvm::isAlnum(C) || C == '+' || C == '.' || C == '-';
    });
}

/// decodes a string according to percent-encoding, e.g., "a%20b" -> "a b".
static std::string decodePercent(llvm::StringRef content) {
    std::string result;
    result.reserve(content.size());

    for(auto iter = content.begin(), send = content.end(); iter != send; ++iter) {
        auto c = *iter;
        if(c == '%' && iter + 2 < send) {
            auto m = *(iter + 1);
            auto n = *(iter + 2);
            if(llvm::isHexDigit(m) && llvm::isHexDigit(n)) {
                result += llvm::hexFromNibbles(m, n);
                iter += 2;
                continue;
            }
        }
        result += c;
    }
    return result;
}

}  // namespace

llvm::Expected<proto::DocumentUri> SourceConverter::toUri(llvm::StringRef fspath) {
    if(!path::is_absolute(fspath))
        return error("file path must be absolute: \"{}\"", fspath);

    return toUriUnchecked(fspath);
};

proto::DocumentUri SourceConverter::toUriUnchecked(llvm::StringRef fspath) {
    llvm::SmallString<128> path("file://");

    for(auto c: fspath) {
        if(c == '\\') {
            path.push_back('/');
        } else if(std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') {
            path.push_back(c);
        } else {
            path.push_back('%');
            path.push_back(llvm::hexdigit(c >> 4));
            path.push_back(llvm::hexdigit(c & 0xF));
        }
    }

    /// TODO:
    /// use `sourceMap` to replace prefix with mapped path.
    // for(const auto& [prefix, newPrefix]: sourceMap) {
    //     if(fspath.starts_with(prefix)) {
    //         path.append(newPrefix); // todo: newPrefix.end_with('/') ???
    //         path.append(fspath.substr(prefix.size()));
    //         break;
    //     }
    // }

    return path.str().str();
};

llvm::Expected<std::string> SourceConverter::toRealPath(llvm::StringRef uri) {
    llvm::StringRef cloned = uri;

    auto pos = cloned.find(':');
    if(pos == llvm::StringRef::npos)
        return error("scheme is missing in URI: {}", cloned);

    auto scheme = cloned.substr(0, pos);
    if(!isValidScheme(scheme)) {
        return error("invalid scheme in URI: {}", cloned);
    }
    cloned = cloned.substr(pos + 1);

    if(cloned.consume_front("//"))
        cloned = cloned.substr(cloned.find('/'));

    auto decoded = decodePercent(cloned);
    llvm::SmallString<128> result;
    if(auto err = fs::real_path(decoded, result))
        return error("failed to resolve URI: {}", uri);

    return result.str().str();
}

std::string SourceConverter::toRealPathUnchecked(llvm::StringRef uri) {
    llvm::StringRef cloned = uri;

    auto pos = cloned.find(':');
    if(pos == llvm::StringRef::npos)
        std::terminate();

    auto scheme = cloned.substr(0, pos);
    cloned = cloned.substr(pos + 1);

    if(cloned.consume_front("//")) {
        cloned = cloned.substr(cloned.find('/'));
    }

    auto decoded = decodePercent(cloned);
    llvm::SmallString<128> result;
    if(auto err = fs::real_path(decoded, result))
        std::terminate();

    return result.str().str();
}

}  // namespace clice
