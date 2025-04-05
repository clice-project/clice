#include "Server/LSPConverter.h"
#include "Support/FileSystem.h"

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

    proto::Position lookup(uint32_t offset) {
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
    PositionEncodingKind encoding;
};

}  // namespace

json::Value LSPConverter::convert(llvm::StringRef content, const feature::Hover& hover) {
    return json::Value(nullptr);
}

json::Value LSPConverter::convert(llvm::StringRef content, const feature::InlayHints& hints) {
    return json::Value(nullptr);
}

json::Value LSPConverter::convert(llvm::StringRef content, const feature::FoldingRanges& foldings) {
    PositionConverter converter(content, encoding());
    converter.toPositions(foldings, [](auto&& folding) { return folding.range; });

    json::Array result;
    for(auto&& folding: foldings) {
        auto [beginOffset, endOffset] = folding.range;
        auto [beginLine, beginChar] = converter.lookup(beginOffset);
        auto [endLine, endChar] = converter.lookup(endOffset);

        auto object = json::Object{
            {"startLine",      beginLine},
            {"startCharacter", beginChar},
            {"endLine",        endLine  },
            {"kind",           "region" },
        };

        result.push_back(std::move(object));
    }
    return result;
}

json::Value LSPConverter::convert(llvm::StringRef content, const feature::DocumentLinks& links) {
    PositionConverter converter(content, encoding());

    json::Array result;
    for(auto& link: links) {
        proto::Range range{
            converter.toPosition(link.range.begin),
            converter.toPosition(link.range.end),
        };

        auto object = json::Object{
            /// The range of document link.
            {"range",  json::serialize(range)},
            /// Target file URI.
            {"target", fs::toURI(link.file)  },
        };

        result.emplace_back(std::move(object));
    }

    return result;
}

json::Value LSPConverter::convert(llvm::StringRef content,
                                  const feature::DocumentSymbols& symbols) {
    PositionConverter converter(content, encoding());

    struct DocumentSymbol {
        std::string name;
        std::string detail;
        SymbolKind kind;
        proto::Range range;
        proto::Range selectionRange;
        std::vector<DocumentSymbol> children;
    };

    json::Array result;

    /// TODO: Implementation.

    return result;
}

json::Value LSPConverter::convert(llvm::StringRef content, const feature::SemanticTokens& tokens) {
    std::vector<std::uint32_t> groups;

    auto addGroup = [&](uint32_t line,
                        uint32_t character,
                        uint32_t length,
                        SymbolKind kind,
                        SymbolModifiers modifiers) {
        groups.emplace_back(line);
        groups.emplace_back(character);
        groups.emplace_back(length);
        groups.emplace_back(kind.value());
        groups.emplace_back(0);
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

    return json::Object{
        /// The actual tokens.
        {"data", json::serialize(groups)},
    };
}

namespace proto {

struct InitializeParams {
    struct ClientInfo {
        std::string name;
        std::string version;
    } clientInfo;

    struct ClientCapabilities {
        struct General {
            std::vector<std::string> positionEncodings;
        } general;
    } capabilities;

    std::vector<WorkspaceFolder> workspaceFolders;
};

struct InitializeResult {
    struct ServerInfo {
        std::string name;
        std::string version;
    } serverInfo;

    struct ServerCapabilities {
        std::string positionEncoding;
        TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Incremental;

        bool declarationProvider = true;
        bool definitionProvider = true;
        bool typeDefinitionProvider = true;
        bool implementationProvider = true;
        bool callHierarchyProvider = true;
        bool typeHierarchyProvider = true;

        bool hoverProvider = true;
        ResolveProvider inlayHintProvider = {true};
        bool foldingRangeProvider = true;
        ResolveProvider documentLinkProvider = {false};
        bool documentSymbolProvider = true;
        SemanticTokenOptions semanticTokensProvider;

        /// TODO:
        /// completionProvider
        /// signatureHelpProvider
        /// codeLensProvider
        /// codeActionProvider
        /// documentFormattingProvider
        /// documentRangeFormattingProvider
        /// renameProvider
        /// diagnosticProvider
    } capabilities;
};

}  // namespace proto

json::Value LSPConverter::initialize(json::Value value) {
    auto params = json::deserialize<proto::InitializeParams>(value);

    auto& encodings = params.capabilities.general.positionEncodings;
    /// Select the first one encoding if any.
    if(encodings.empty()) {
        kind = PositionEncodingKind::UTF16;
    } else if(encodings[0] == "utf-8") {
        kind = PositionEncodingKind::UTF8;
    } else if(encodings[0] == "utf-16") {
        kind = PositionEncodingKind::UTF16;
    } else if(encodings[0] == "utf-32") {
        kind = PositionEncodingKind::UTF32;
    }

    workspacePath = fs::toPath(params.workspaceFolders[0].uri);

    proto::InitializeResult result{
        .serverInfo = {"clice", "0.0.1"},
        .capabilities = {
                       .positionEncoding = encodings.empty() ? "utf-16" : encodings[0],
                       }
    };

    auto& semanticTokensProvider = result.capabilities.semanticTokensProvider;
    for(auto name: SymbolKind::all()) {
        std::string type{name};
        type[0] = std::tolower(type[0]);
        semanticTokensProvider.legend.tokenTypes.emplace_back(std::move(type));
    }

    return json::serialize(result);
}

}  // namespace clice
