#include "Server/Cache.h"

namespace clice {

void CacheController::loadFromDisk() {

}

void CacheController::saveToDisk() {

}

async2::Task<> CacheController::prepare(CompilationParams& params) {
    /// 首先要区分这个文件是不是 module file.
    bool isModule = false;

    if(isModule){
        /// Update dependent mods ....
    }

    /// 准备构建 PCH ...

    co_return;
}

}  // namespace clice
