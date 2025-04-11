#pragma once

#include "llvm/ADT/StringMap.h"

namespace clice {

class Document {};

/// Responsible for all opened files.
class DocumentManager {
public:

private:
    /// TODO: Use an LRU for this.
    llvm::StringMap<Document> documents;
};

}  // namespace clice
