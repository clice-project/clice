#pragma once

#include "RawIndex.h"
#include "IncludeGraph.h"

namespace clice::index::memory {

class TUIndex : public RawIndex {
public:

public:
    /// The time of building this index.
    std::int64_t time;

    /// The include graph of this index.
    IncludeGraph graph;
};

}  // namespace clice::index::memory
