#include <Support/URI.h>

namespace clice {

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
    for(auto iter = content.begin(), sent = content.end(); iter != sent; ++iter) {
        auto c = *iter;
        if(c == '%' && iter + 2 < sent) {
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

llvm::Expected<URI> URI::parse(llvm::StringRef content) {
    URI result("", "", "");
    llvm::StringRef uri = content;
    auto pos = uri.find(':');
    if(pos == llvm::StringRef::npos) {
        return error("scheme is missing in URI: {}", content);
    } else {
        result.m_scheme = uri.substr(0, pos);
        if(!isValidScheme(result.m_scheme)) {
            return error("invalid scheme in URI: {}", content);
        }
        uri = uri.substr(pos + 1);
    }

    if(uri.consume_front("//")) {
        pos = uri.find('/');
        result.m_authority = uri.substr(0, pos);
        uri = uri.substr(pos);
    }

    result.m_body = decodePercent(uri);

    return result;
}

}  // namespace clice
