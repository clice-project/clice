#pragma once

#include <string>
#include <expected>

#include "Support/Support.h"

namespace clice {

class URI {
public:
    URI(llvm::StringRef scheme, llvm::StringRef authority, llvm::StringRef path) :
        m_scheme(scheme), m_authority(authority), m_body(path) {}

    URI(const URI&) = default;

    bool operator== (const URI&) const = default;

    /// Construct a URI object from the given file path.
    static URI from(llvm::StringRef file);

    /// Parse the given URI string to create a URI object.
    static std::expected<URI, std::string> parse(llvm::StringRef content);

    /// Same as `parse`, but will crash if failed.
    static std::string resolve(llvm::StringRef content);

    /// Returns decoded scheme e.g. "https"
    llvm::StringRef scheme() const {
        return m_scheme;
    }

    /// Returns decoded authority e.g. "reviews.llvm.org"
    llvm::StringRef authority() const {
        return m_authority;
    }

    /// Returns decoded body e.g. "/D41946"
    llvm::StringRef body() const {
        return m_body;
    }

    std::string toString() const {
        return std::format("{}://{}{}", m_scheme, m_authority, m_body);
    }

private:
    std::string m_scheme;
    std::string m_authority;
    std::string m_body;
};

}  // namespace clice
