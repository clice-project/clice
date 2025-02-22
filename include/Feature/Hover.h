#pragma once

#include "Basic/Document.h"
#include "AST/SymbolKind.h"

namespace clice {

class ASTInfo;

namespace config {

/// For a full memory layout infomation, the render kind decides how to display the value. By
/// default, show both decimal and hexadecimal. e.g:
///         size = 4 (0x4), align = 4 (0x4), offset: 0 (0x0)
/// while show decimal only:
///         size = 4, align = 4, offset: 0
/// while show hexadecimal only:
///         size = 0x4, align = 0x4, offset: 0x0
///
/// And bit field is always displayed in decimal.
///         size = 1 bit (+5 bits padding), align = 1 byte, offset: 4 byte + 2 bit
enum class MemoryLayoutRenderKind : uint8_t {
    Both = 0,
    Decimal,
    Hexadecimal,
};

struct HoverOption {
    /// The maximum number of fields to show in the hover of a class/struct/enum. 0 means show all.
    uint16_t maxFieldsCount = 0;

    /// TODO:
    /// The maximum number of derived classes to show in the hover of a pure virtual class.
    // uint16_t maxDerivedClassNum;

    /// Decide how to render the memory layout.
    MemoryLayoutRenderKind memoryLayoutRenderKind = MemoryLayoutRenderKind::Both;

    /// Show associated document.
    bool documentation : 1 = true;

    /// TODO:
    /// Show overloaded virtual method for class/struct.
    bool overloadVirtualMethod : 1 = true;

    /// Show documentation link for key words, this will link to corresponding page of
    /// `https://en.cppreference.com/w/cpp/keyword/`.
    bool keywords : 1 = true;

    /// TODO:
    /// Show links instead of codeblock in hover information for mentioned symbols.
    bool useLink : 1 = true;
};

}  // namespace config

namespace feature::hover {

/// TODO:
/// Implement the action for hovering over elements.
// struct HoverAction {
//     // Goto type
//     // Find reference
// };

struct Result {
    std::string markdown;
};

/// Get the hover information of a declaration with given option.
Result hoverInfo(const clang::Decl* decl, const config::HoverOption& option);

}  // namespace feature::hover

}  // namespace clice
