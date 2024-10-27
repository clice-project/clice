#include <vector>
#include <llvm/ADT/StringMap.h>

namespace clice {

/// responsible for:
/// - build PCH and PCM for specific file
/// - update PCH and PCM
class CacheManager {
public:

private:
    // file -> dependencies files
    // check modification time of file and dependencies files to decide whether to rebuild PCH and
    // PCM
    llvm::StringMap<std::vector<std::string>> caches;
};

}  // namespace clice
