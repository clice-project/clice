#include <Support/SourceCode.h>

namespace clice {

namespace {

// Here be dragons. LSP positions use columns measured in *UTF-16 code units*!
// Clangd uses UTF-8 and byte-offsets internally, so conversion is nontrivial.

// Iterates over unicode codepoints in the (UTF-8) string. For each,
// invokes CB(UTF-8 length, UTF-16 length), and breaks if it returns true.
// Returns true if CB returned true, false if we hit the end of string.
//
// If the string is not valid UTF-8, we log this error and "decode" the
// text in some arbitrary way. This is pretty sad, but this tends to happen deep
// within indexing of headers where clang misdetected the encoding, and
// propagating the error all the way back up is (probably?) not be worth it.
template <typename Callback>
static bool iterateCodepoints(llvm::StringRef u8string, const Callback& callback) {
    bool LoggedInvalid = false;
    // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
    // Astral codepoints are encoded as 4 bytes in UTF-8, starting with 11110xxx.
    for(size_t I = 0; I < u8string.size();) {
        unsigned char C = static_cast<unsigned char>(u8string[I]);
        if(LLVM_LIKELY(!(C & 0x80))) {  // ASCII character.
            if(callback(1, 1))
                return true;
            ++I;
            continue;
        }
        // This convenient property of UTF-8 holds for all non-ASCII characters.
        size_t UTF8Length = llvm::countl_one(C);
        // 0xxx is ASCII, handled above. 10xxx is a trailing byte, invalid here.
        // 11111xxx is not valid UTF-8 at all, maybe some ISO-8859-*.
        if(LLVM_UNLIKELY(UTF8Length < 2 || UTF8Length > 4)) {
            if(!LoggedInvalid) {
                std::terminate();
                LoggedInvalid = true;
            }
            // We can't give a correct result, but avoid returning something wild.
            // Pretend this is a valid ASCII byte, for lack of better options.
            // (Too late to get ISO-8859-* right, we've skipped some bytes already).
            if(callback(1, 1))
                return true;
            ++I;
            continue;
        }
        I += UTF8Length;  // Skip over all trailing bytes.
        // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
        // Astral codepoints are encoded as 4 bytes in UTF-8 (11110xxx ...)
        if(callback(UTF8Length, UTF8Length == 4 ? 2 : 1))
            return true;
    }
    return false;
}

}  // namespace

std::size_t length(llvm::StringRef u8string, Encoding encoding) {
    size_t count = 0;
    switch(encoding) {
        case Encoding::UTF8: {
            count = u8string.size();
            break;
        }
        case Encoding::UTF16: {
            iterateCodepoints(u8string, [&](int U8Len, int U16Len) {
                count += U16Len;
                return false;
            });
            break;
        }
        case Encoding::UTF32: {
            iterateCodepoints(u8string, [&](int U8Len, int U16Len) {
                ++count;
                return false;
            });
            break;
        }
    }
    return count;
}

}  // namespace clice
