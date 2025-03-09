#include "Server/LSPConverter.h"
#include "Basic/SourceConverter.h"

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
    PositionConverter(llvm::StringRef content, proto::PositionEncodingKind encoding) :
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
    void toPositions(Range&& range, Proj&& proj) {
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

    proto::Position toPosition2(uint32_t offset) {
        auto it = cache.find(offset);
        assert(it != cache.end() && "Offset is not cached");
        return it->second;
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
    proto::PositionEncodingKind encoding;
};

}  // namespace

proto::InitializeResult LSPConverter::initialize(json::Value value) {
    params = json::deserialize<proto::InitializeParams>(value);

    proto::InitializeResult result = {};
    result.serverInfo.name = "clice";
    result.serverInfo.version = "0.0.1";

    auto& semantictokens = result.capabilities.semanticTokensProvider;
    for(auto& name: SymbolKind::all()) {
        std::string type{name};
        type[0] = std::tolower(type[0]);
        semantictokens.legend.tokenTypes.emplace_back(std::move(type));
    }

    return result;
}

llvm::StringRef LSPConverter::workspace() {
    if(workspacePath.empty()) {
        workspacePath = SourceConverter::toPath(params.workspaceFolders[0].uri);
    }
    return workspacePath;
}

proto::SemanticTokens LSPConverter::transform(llvm::StringRef content,
                                              llvm::ArrayRef<feature::SemanticToken> tokens) {
    proto::SemanticTokens result;

    auto addGroup = [&](uint32_t line,
                        uint32_t character,
                        uint32_t length,
                        SymbolKind kind,
                        SymbolModifiers modifiers) {
        result.data.emplace_back(line);
        result.data.emplace_back(character);
        result.data.emplace_back(length);
        result.data.emplace_back(kind.value());

        /// FIXME:
        result.data.emplace_back(0);
    };

    PositionConverter converter(content, encoding());
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
            addGroup(line, character, length, token.kind, token.modifiers);
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
                        remeasure(subContent.substr(lastLineOffset, lineLength), encoding());
                    addGroup(line, character, length, token.kind, token.modifiers);

                    lastLineOffset += lineLength;
                    lineLength = 0;
                }
            }

            /// Process the last line if it's not empty.
            if(lineLength > 0) {
                std::uint32_t length = remeasure(subContent.substr(lastLineOffset), encoding());
                addGroup(1, 0, length, token.kind, token.modifiers);
            }
        }

        lastLine = endLine;
        lastChar = beginChar;
    }

    return result;
}

std::vector<proto::FoldingRange>
    LSPConverter::transform(llvm::StringRef content,
                            llvm::ArrayRef<feature::FoldingRange> foldings) {
    std::vector<proto::FoldingRange> result;

    PositionConverter converter(content, encoding());
    converter.toPositions(foldings, [](auto&& folding) { return folding.range; });

    for(auto&& folding: foldings) {
        auto [beginOffset, endOffset] = folding.range;
        auto [beginLine, beginChar] = converter.toPosition2(beginOffset);
        auto [endLine, endChar] = converter.toPosition2(endOffset);

        result.emplace_back(proto::FoldingRange{
            .startLine = beginLine,
            .startCharacter = beginChar,
            .endLine = endLine,
            /// FIXME: Figure out how to handle end character.
            .endCharacter = endChar - 1,
            .kind = proto::FoldingRangeKind::Region,
            .collapsedText = folding.text,
        });
    }

    return result;
}

LSPConverter::Result LSPConverter::convert(llvm::StringRef path,
                                           llvm::ArrayRef<feature::SemanticToken> tokens) {
    auto file = co_await async::fs::read(path.str());
    if(!file) {
        co_return json::Value(nullptr);
    }
    llvm::StringRef content = *file;
    co_return json::serialize(transform(content, tokens));
}

LSPConverter::Result LSPConverter::convert(llvm::StringRef path,
                                           llvm::ArrayRef<feature::FoldingRange> foldings) {
    auto file = co_await async::fs::read(path.str());
    if(!file) {
        co_return json::Value(nullptr);
    }
    llvm::StringRef content = *file;
    co_return json::serialize(transform(content, foldings));
}

LSPConverter::Result LSPConverter::convert(const feature::Hover& hover) {
    /// FIXME: Implement hover information render here.
    co_return json::Value("");
}

}  // namespace clice
