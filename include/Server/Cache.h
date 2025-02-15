#pragma once

#include <deque>

#include "Async/Async.h"
#include "Database.h"
#include "Compiler/Module.h"
#include "Compiler/Preamble.h"

#include "llvm/ADT/StringMap.h"

namespace clice {

struct CacheOption {
    /// The directory to store the cache files.
    std::string dir;
};

/// This class is responsible for PCH and PCM building.
class CacheController {
public:
    CacheController(CacheOption& option, CompilationDatabase& database);

    /// Generate `cache.json` to store the cache information.
    void loadFromDisk();

    /// Load the cache information from `cache.json`.
    void saveToDisk();

    /// Complete the PCH or PCM information required for the compilation arguments.
    /// If no suitable PCH or PCM is available, a build will be triggered.
    async::Task<> prepare(CompilationParams& params);

    async::Task<> updatePCH();

private:
    const CacheOption& option;

    CompilationDatabase& database;

    struct CachedPCHInfo : PCHInfo {
        /// The hash of the preamble, for fast comparison.
        std::uint64_t hash;

        /// The reference count of this PCH. When server exit, all PCH with zero
        /// reference count will be removed.
        std::uint32_t reference;
    };

    /// All PCHs.
    std::deque<CachedPCHInfo> pchs;

    /// A map between source file and its PCH.
    llvm::StringMap<CachedPCHInfo*> pchMap;

    /// [module name] -> [PCMInfo]
    llvm::StringMap<PCMInfo> pcms;
};

}  // namespace clice
