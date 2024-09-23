#pragma once

#include "CSIF.h"

namespace clice {

// We use an efficient way to pack the CSIF structure into a binary format.
//
// The serialization process follows these steps:
// 1. Write the `CSIF` structure directly into the binary buffer.
// 2. For each reference member (e.g., arrays and strings):
//    a. Write the referenced data (array elements or string characters) into the buffer.
//    b. Replace the pointer in the `CSIF` structure with the offset pointing to the actual data.
//
// Data Layout in the binary buffer:
// | CSIF structure | array offsets | string offsets | array data | string data |
//
// - `CSIF structure`: The first `sizeof(CSIF)` bytes store the `CSIF` structure itself.
// - `array offsets`: Offsets pointing to the actual array data stored later in the buffer.
// - `string offsets`: Offsets pointing to the actual string data stored later in the buffer.
// - `array data`: Contains all array elements, stored with 8-byte alignment to improve access efficiency.
// - `string data`: Contains all string characters, stored sequentially.

/// Pack the CSIF into a binary buffer.
std::unique_ptr<char[]> pack(const CSIF& csif);

/// Unpack the binary buffer into a CSIF.
/// NOTE: the data should be mutable. when the first load it,
/// We need to replace all offset to actual pointer.
CSIF unpack(char* data);

}  // namespace clice
