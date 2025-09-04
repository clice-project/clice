#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"
#include <memory>

namespace llvm {
class MemoryBuffer;
}

namespace clice {

/**
 *  @brief Compresses a pre-compiled file (PCH/PCM) using LZ4 and removes the original file.
 * 
 * The compressed file has a custom format: an 8-byte header for the
 * original size (uint64_t), followed by the LZ4 compressed data.
 * 
 * @param path The path to the pre-compiled file to compress.
 */
void compressPreCompiledFile(std::string path);
/**
 *  @brief Compresses a file using LZ4 and saves it to an output path.
 * 
 * The compressed file has a custom format: an 8-byte header for the
 * original size (uint64_t), followed by the LZ4 compressed data.
 * 
 * @param inputPath The path to the file to compress.
 * @param outputPath The path where the compressed file will be saved.
 * @return true on success, false on failure (e.g., input file not found).
 */
bool compressToFile(std::string inputPath, std::string outputPath);
/**
 *  @brief Decompresses a file compressed with `compressToFile`.
 * 
 * The function reads the custom format: an 8-byte header for the
 * original size (uint64_t), followed by the LZ4 compressed data.
 * 
 * @param path The path to the compressed file.
 * @return A unique pointer to a MemoryBuffer containing the decompressed data,
 *         or nullptr on failure (e.g., file not found or decompression error).
 */
std::unique_ptr<llvm::MemoryBuffer> decompressFile(std::string path);

} // namespace clice