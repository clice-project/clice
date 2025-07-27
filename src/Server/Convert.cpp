#include "Server/Convert.h"
#include "Server/Protocol.h"
#include "Support/Ranges.h"
#include "Support/JSON.h"
#include "Support/Format.h"

namespace clice::proto {

namespace {

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
bool iterateCodepoints(llvm::StringRef content, const Callback& callback) {
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

/// Convert a proto::Position to a file offset in the content with the specified encoding kind.
std::uint32_t toOffset(llvm::StringRef content,
                       PositionEncodingKind kind,
                       proto::Position position) {
    std::uint32_t offset = 0;
    for(auto i = 0; i < position.line; i++) {
        auto pos = content.find('\n');
        assert(pos != llvm::StringRef::npos && "Line value is out of range");

        offset += pos + 1;
        content = content.substr(pos + 1);
    }

    /// Drop the content after the line.
    content = content.take_until([](char c) { return c == '\n'; });
    assert(position.character <= content.size() && "Character value is out of range");

    if(position.character == 0) {
        return offset;
    }

    if(kind == PositionEncodingKind::UTF8) {
        offset += position.character;
        return offset;
    }

    if(kind == PositionEncodingKind::UTF16) {
        iterateCodepoints(content, [&](std::uint32_t utf8Length, std::uint32_t utf16Length) {
            assert(position.character >= utf16Length && "Character value is out of range");
            position.character -= utf16Length;
            offset += utf8Length;
            return position.character != 0;
        });
        return offset;
    }

    if(kind == PositionEncodingKind::UTF32) {
        iterateCodepoints(content, [&](std::uint32_t utf8Length, std::uint32_t) {
            assert(position.character >= 1 && "Character value is out of range");
            position.character -= 1;
            offset += utf8Length;
            return position.character != 0;
        });
        return offset;
    }

    std::unreachable();
}

/// Remeasure the length (character count) of the content with the specified encoding kind.
std::uint32_t remeasure(llvm::StringRef content, PositionEncodingKind kind) {
    if(kind == PositionEncodingKind::UTF8) {
        return content.size();
    }

    if(kind == PositionEncodingKind::UTF16) {
        std::uint32_t length = 0;
        iterateCodepoints(content, [&](std::uint32_t, std::uint32_t utf16Length) {
            length += utf16Length;
            return true;
        });
        return length;
    }

    if(kind == PositionEncodingKind::UTF32) {
        std::uint32_t length = 0;
        iterateCodepoints(content, [&](std::uint32_t, std::uint32_t) {
            length += 1;
            return true;
        });
        return length;
    }

    std::unreachable();
}

class PositionConverter {
public:
    PositionConverter(llvm::StringRef content, PositionEncodingKind encoding) :
        content(content), encoding(encoding) {}

    /// Convert a offset to a proto::Position with given encoding.
    /// The input offset must be UTF-8 encoded and in order.
    proto::Position toPosition(uint32_t offset) {
        assert(offset <= content.size() && "Offset is out of range");
        assert(offset >= lastInput && "Offset must be in order");

        /// Fast path: return the last output.
        if(offset == lastInput) [[unlikely]] {
            return lastOutput;
        }

        /// The length of the current line.
        std::uint32_t lineLength = 0;

        /// Move the line offset to the current line.
        for(std::uint32_t i = lastLineOffset; i < offset; i++) {
            lineLength += 1;
            if(content[i] == '\n') {
                line += 1;
                lastLineOffset += lineLength;
                lineLength = 0;
            }
        }

        /// Get the content of the current line.
        auto lineContent = content.substr(lastLineOffset, lineLength);
        auto position = proto::Position{
            .line = line,
            .character = remeasure(lineContent, encoding),
        };

        /// Cache the result.
        lastInput = offset;
        lastOutput = position;

        return position;
    }

    template <typename Range, typename Proj>
    void to_positions(Range&& range, Proj&& proj) {
        std::vector<uint32_t> offsets;
        for(auto&& item: range) {
            auto [begin, end] = proj(item);
            offsets.emplace_back(begin);
            offsets.emplace_back(end);
        }

        ranges::sort(offsets);

        for(auto&& offset: offsets) {
            if(auto it = cache.find(offset); it == cache.end()) {
                cache.try_emplace(offset, toPosition(offset));
            }
        }
    }

    proto::Position lookup(uint32_t offset) {
        auto it = cache.find(offset);
        assert(it != cache.end() && "Offset is not cached");
        return it->second;
    }

    proto::Range lookup(LocalSourceRange range) {
        auto it = cache.find(range.begin);
        assert(it != cache.end() && "Offset is not cached");
        auto begin = it->second;
        it = cache.find(range.end);
        assert(it != cache.end() && "Offset is not cached");
        auto end = it->second;
        return proto::Range{begin, end};
    }

private:
    std::uint32_t line = 0;
    /// The offset of the last line end.
    std::uint32_t lastLineOffset = 0;

    /// The input offset of last call.
    std::uint32_t lastInput = 0;
    proto::Position lastOutput = {0, 0};

    llvm::DenseMap<std::uint32_t, proto::Position> cache;

    llvm::StringRef content;
    PositionEncodingKind encoding;
};

}  // namespace

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::SemanticToken> tokens) {
    std::vector<std::uint32_t> groups;

    auto add_token = [&](uint32_t line,
                         uint32_t character,
                         uint32_t length,
                         clice::SymbolKind kind,
                         SymbolModifiers modifiers) {
        groups.emplace_back(line);
        groups.emplace_back(character);
        groups.emplace_back(length);
        groups.emplace_back(kind.value());
        groups.emplace_back(0);
    };

    PositionConverter converter(content, kind);
    std::uint32_t last_line = 0;
    std::uint32_t last_char = 0;

    for(auto& token: tokens) {
        auto [begin_offset, end_offset] = token.range;
        auto [begin_line, begin_char] = converter.toPosition(begin_offset);
        auto [end_line, end_char] = converter.toPosition(end_offset);

        if(begin_line == end_line) [[likely]] {
            std::uint32_t line = begin_line - last_line;
            std::uint32_t character = (line == 0 ? begin_char - last_char : begin_char);
            std::uint32_t length = end_char - begin_char;
            add_token(line, character, length, token.kind, token.modifiers);
        } else {
            /// If the token spans multiple lines, split it into multiple tokens.
            auto sub_content = content.substr(begin_offset, end_offset - begin_offset);

            /// The first line is special.
            bool isFirst = true;
            /// The offset of the last line end.
            std::uint32_t last_line_offset = 0;
            /// The length of the current line.
            std::uint32_t line_length = 0;

            for(auto c: sub_content) {
                line_length += 1;
                if(c == '\n') {
                    std::uint32_t line;
                    std::uint32_t character;

                    if(isFirst) [[unlikely]] {
                        line = begin_line - last_line;
                        character = (line == 0 ? begin_char - last_char : begin_char);
                        isFirst = false;
                    } else {
                        line = 1;
                        character = 0;
                    }

                    std::uint32_t length =
                        remeasure(sub_content.substr(last_line_offset, line_length), kind);
                    add_token(line, character, length, token.kind, token.modifiers);

                    last_line_offset += line_length;
                    line_length = 0;
                }
            }

            /// Process the last line if it's not empty.
            if(line_length > 0) {
                std::uint32_t length = remeasure(sub_content.substr(last_line_offset), kind);
                add_token(1, 0, length, token.kind, token.modifiers);
            }
        }

        last_line = end_line;
        last_char = begin_char;
    }

    auto object = json::Object{
        /// The actual tokens.
        {"data", json::serialize(groups)}
    };
    return std::format("{}", json::Value(std::move(object)));
}

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::CompletionItem> items) {
    PositionConverter converter(content, kind);
    converter.to_positions(items, [](auto& item) { return item.edit.range; });

    json::Array result;

    for(auto& item: items) {
        json::Object object{
            {"label", item.label},
            {"kind", static_cast<int>(item.kind)},
            {"textEdit",
             json::Object{
                 {"newText", item.edit.text},
                 {"range", json::serialize(converter.lookup(item.edit.range))},
             }},
            {"sortText", std::format("{}", item.score)},
        };
        result.emplace_back(std::move(object));
    }

    return std::format("{}", json::Value(std::move(result)));
}

}  // namespace clice::proto
