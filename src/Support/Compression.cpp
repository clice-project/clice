#include "Support/Compression.h"
#include "llvm/Support/MemoryBuffer.h"
#include "lz4.h"
#include <string>
#include "Support/FileSystem.h"
#include "Support/Logger.h"

namespace clice {

void compressPreCompiledFile(llvm::StringRef path) {
    if(!fs::exists(path)) {
        log::warn("PreCompiledFile does not exist: {}", path);
    } else if(!compressToFile(path, path + ".lz4")) {
        log::warn("Fail to compress PreCompiledFile: {}", path);
    } else {
        if(auto ec = fs::remove(path)) {
            log::warn("Fail to remove original PreCompiledFile: {}. Reason: {}",
                      path,
                      ec.message());
        }
    }
}

bool compressToFile(std::string inputPath, std::string outputPath) {
    auto content = fs::read(inputPath);
    if(!content) {
        return false;
    }
    uint64_t originalSize = content->size();
    int maxCompressedSize = LZ4_compressBound(originalSize);
    std::vector<char> compressed(maxCompressedSize + sizeof(uint64_t));

    // Our custom format: Prepend the 8-byte original size as a header
    // before the compressed data.
    memcpy(compressed.data(), &originalSize, sizeof(uint64_t));

    int compressedDataSize = LZ4_compress_default(content->data(),
                                                  compressed.data() + sizeof(uint64_t),
                                                  originalSize,
                                                  maxCompressedSize);
    if(compressedDataSize <= 0) {
        return false;
    }

    size_t totalSize = compressedDataSize + sizeof(uint64_t);

    llvm::StringRef dataToWrite(compressed.data(), totalSize);
    auto result = fs::write(outputPath, dataToWrite);
    return result.has_value();
}

std::unique_ptr<llvm::MemoryBuffer> decompressFile(std::string path) {
    auto content = fs::read(path);
    if(!content) {
        log::warn("Fail to read compressed file: {}", path);
        return nullptr;
    }

    if(content->size() < sizeof(uint64_t)) {
        log::warn("Fail to read compressed file: {}", path);
        return nullptr;  // Not enough data for size header
    }

    // Our custom format: Prepend the 8-byte original size as a header
    // before the compressed data.
    uint64_t originalSize;
    memcpy(&originalSize, content->data(), sizeof(uint64_t));

    const char* compressedData = content->data() + sizeof(uint64_t);
    size_t compressedDataSize = content->size() - sizeof(uint64_t);

    auto buffer = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(originalSize);
    if(!buffer) {
        log::warn("Fail to allocate memory buffer for decompression: {}", path);
        return nullptr;
    }

    int decompressedSize = LZ4_decompress_safe(compressedData,
                                               buffer->getBufferStart(),
                                               compressedDataSize,
                                               originalSize);

    if(decompressedSize < 0) {  // LZ4 returns negative on error
        log::warn("Fail to decompress file: {}", path);
        return nullptr;
    }

    if(static_cast<uint64_t>(decompressedSize) != originalSize) {
        // This case should ideally not happen if the stored size is correct
        return nullptr;
    }

    return buffer;
}

}  // namespace clice
