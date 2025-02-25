#include "Server/LSPConverter.h"

namespace clice {

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
                       proto::PositionEncodingKind kind,
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

/// Remeasure the length (character count) of the content with the specified encoding kind.
std::uint32_t remeasure(llvm::StringRef content, proto::PositionEncodingKind kind) {
    if(kind == proto::PositionEncodingKind::UTF8) {
        return content.size();
    }

    if(kind == proto::PositionEncodingKind::UTF16) {
        std::uint32_t length = 0;
        iterateCodepoints(content, [&](std::uint32_t, std::uint32_t utf16Length) {
            length += utf16Length;
            return true;
        });
        return length;
    }

    if(kind == proto::PositionEncodingKind::UTF32) {
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
    PositionConverter(llvm::StringRef content, proto::PositionEncodingKind kind) :
        content(content), kind(kind) {}

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
            .character = remeasure(lineContent, kind),
        };

        /// Cache the result.
        lastInput = offset;
        lastOutput = position;

        return position;
    }

private:
    std::uint32_t line = 0;
    /// The offset of the last line end.
    std::uint32_t lastLineOffset = 0;

    /// The input offset of last call.
    std::uint32_t lastInput = 0;
    proto::Position lastOutput = {0, 0};

    llvm::StringRef content;
    proto::PositionEncodingKind kind;
};

}  // namespace

LSPConverter::Result LSPConverter::convert(llvm::StringRef path,
                                           llvm::ArrayRef<feature::SemanticToken> tokens) {
    /// FIXME: Use a better way to handle file content.
    auto file = co_await async::fs::read(path.str());
    if(!file) {
        co_return json::Value(nullptr);
    }
    llvm::StringRef content = *file;

    struct SemanticTokens {
        /// The actual tokens.
        std::vector<std::uint32_t> data;

        void add(std::uint32_t line,
                 std::uint32_t character,
                 std::uint32_t length,
                 SymbolKind kind,
                 SymbolModifiers modifiers) {
            /// FIXME: Add a map between lsp kinds and our kinds.
            /// [line, character, length, tokenType, tokenModifiers]
            data.emplace_back(line);
            data.emplace_back(character);
            data.emplace_back(length);
            data.emplace_back(kind.value());
            data.emplace_back(modifiers.value());
        }
    };

    SemanticTokens result;

    PositionConverter converter(content, kind);
    std::uint32_t lastLine = 0;
    std::uint32_t lastChar = 0;

    for(auto& token: tokens) {
        auto [beginOffset, endOffset] = token.range;
        auto [beginLine, beginChar] = converter.toPosition(beginOffset);
        auto [endLine, endChar] = converter.toPosition(endOffset);

        if(beginLine == endLine) [[likely]] {
            std::uint32_t line = beginLine - lastLine;
            std::uint32_t character = (line == 0 ? beginChar - lastChar : beginChar);
            std::uint32_t length = endChar - beginChar;
            result.add(line, character, length, token.kind, token.modifiers);
        } else {
            /// If the token spans multiple lines, split it into multiple tokens.
            auto subContent = content.substr(beginOffset, endOffset - beginOffset);

            /// The first line is special.
            bool isFirst = true;
            /// The offset of the last line end.
            std::uint32_t lastLineOffset = 0;
            /// The length of the current line.
            std::uint32_t lineLength = 0;

            for(auto c: subContent) {
                lineLength += 1;
                if(c == '\n') {
                    std::uint32_t line;
                    std::uint32_t character;

                    if(isFirst) [[unlikely]] {
                        line = beginLine - lastLine;
                        character = (line == 0 ? beginChar - lastChar : beginChar);
                        isFirst = false;
                    } else {
                        line = 1;
                        character = 0;
                    }

                    std::uint32_t length =
                        remeasure(subContent.substr(lastLineOffset, lineLength), kind);
                    result.add(line, character, length, token.kind, token.modifiers);

                    lastLineOffset += lineLength;
                    lineLength = 0;
                }
            }

            /// Process the last line if it's not empty.
            if(lineLength > 0) {
                std::uint32_t length = remeasure(subContent.substr(lastLineOffset), kind);
                result.add(1, 0, length, token.kind, token.modifiers);
            }
        }

        lastLine = endLine;
        lastChar = endChar;
    }

    co_return json::serialize(result);
}

LSPConverter::Result LSPConverter::convert(const feature::Hover& hover) {
    /// FIXME: Implement hover information render here.
    co_return json::Value("");
}

// namespace {
// //
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintKind
// struct LspHintKind : refl::Enum<LspHintKind> {
//     enum Kind : uint8_t {
//         Invalid = 0,
//         Type = 1,
//         Parameter = 2,
//     };

//     using Enum::Enum;

//     static LspHintKind fromKind(feature::InlayHintKind kind) {
//         return kind.isLspTypeKind() ? Type : Parameter;
//     }
// };

// //
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintLabelPart
// struct LabelPart {
//     /// The label of the inlay hint.
//     std::string value;

//     /// The tooltip text when you hover over this label part.  Depending on
//     /// the client capability `inlayHint.resolveSupport` clients might resolve
//     /// this property late using the resolve request.
//     /// TODO:
//     /// MarkupContent tooltip;

//     /// An optional source code location that represents this label part. This part will become
//     /// a clickable link that resolves to the definition of the symbol at the given location
//     /// (not necessarily the location itself), it shows the hover that shows at the given
//     /// location,
//     Location location;

//     /// TODO:
//     // Command command;
// };

// //
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHint
// struct InlayHint {
//     /// The position of this hint.
//     proto::Position position;

//     /// The label of this hint.
//     std::vector<LabelPart> lables;

//     /// The kind of this hint.
//     LspHintKind kind;

//     /// TODO:
//     /// Optional text edits that are performed when accepting this inlay hint.
//     // std::vector<TextEdit> textEdits;

//     /// Render padding before the hint.
//     bool paddingLeft = false;

//     /// Render padding after the hint.
//     bool paddingRight = false;

//     /// TODO:
//     // LspAny data;
// };

// }  // namespace

LSPConverter::Result LSPConverter::convert(const std::vector<feature::InlayHint>& hints,
                                           const config::InlayHintOption option) {
    // std::vector<InlayHint> lspHints;

    /// FIXME: Convert `feature::InlayHint` to Json response. Handle the optional field `LabelPart::location`.
    /// co_return json::serialize(lspHints);

    co_return json::Value("");
}

}  // namespace clice
