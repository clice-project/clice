#pragma once

#include <bitset>
#include <cstdint>
#include <deque>
#include <vector>

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/BitVector.h"

namespace clice::index {

struct Contextual {
    /// The actual element id,
    std::uint32_t element_id;

    constexpr inline static std::uint32_t FLAG = (1ull << 31);

    static Contextual from(bool is_dependent, std::uint32_t offset) {
        Contextual ctx;
        ctx.element_id = offset;
        if(!is_dependent) {
            ctx.element_id |= FLAG;
        }
        return ctx;
    }

    bool is_dependent() {
        return (element_id & FLAG) == 0;
    }

    std::uint32_t offset() {
        return element_id & ~FLAG;
    }
};

}  // namespace clice::index
