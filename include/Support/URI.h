#pragma once

#include <string>
#include <Support/Error.h>

namespace clice {

class URI {
public:
    URI(llvm::StringRef scheme, llvm::StringRef authority, llvm::StringRef path) :
        m_scheme(scheme), m_authority(authority), m_body(path) {}

    URI(const URI&) = default;

    bool operator== (const URI&) const = default;

    /// Returns decoded scheme e.g. "https"
    llvm::StringRef scheme() const { return m_scheme; }

    /// Returns decoded authority e.g. "reviews.lvm.org"
    llvm::StringRef authority() const { return m_authority; }

    /// Returns decoded body e.g. "/D41946"
    llvm::StringRef body() const { return m_body; }

    static llvm::Expected<URI> parse(llvm::StringRef content);

    static std::string resolve(llvm::StringRef content);

private:
    std::string m_scheme;
    std::string m_authority;
    std::string m_body;
};

}  // namespace clice
