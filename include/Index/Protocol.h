#pragma once

#include <Support/ADT.h>

namespace clice {

struct SymbolID {};

struct Range {};

enum class SymbolKind : uint8_t {

};

struct Relation {
    SymbolID target;
    bool isReference = false;
    bool isImplementation = false;
    bool isTypeDefinition = false;
    bool isBase = false;
    bool isOverride = false;
};

struct Symbol {
    SymbolID symbol;
    SymbolKind kind;
    StringRef displayName;
    StringRef document;
    std::vector<Relation> relations;
};

enum class SymbolRole : uint8_t {
    Definition,
    Import,
    WriteAccess,
    ReadAccess,
    ForwardDeclaration,
};

struct Occurrence {
    SymbolID symbol;
    Range range;
    SymbolRole role;
};

struct Document {
    StringRef uri;
    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
};

};  // namespace clice
