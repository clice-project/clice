#include <Basic/URI.h>

#include <Support/ADT.h>
#include <Support/FileSystem.h>
#include <exception>

namespace clice {

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

}  // namespace

URI URI::from(llvm::StringRef file) {
    if(!path::is_absolute(file)) {
        std::terminate();
    }

    llvm::SmallString<128> path;

    for(auto c: file) {
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

    return URI("file", "", path);
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

std::string URI::resolve(llvm::StringRef content) {
    auto uri = parse(content);
    if(!uri) {
        std::terminate();
    }
    llvm::SmallString<128> result;
    if(auto err = fs::real_path(uri->body(), result)) {
        std::terminate();
    }
    return result.str().str();
}

}  // namespace clice
