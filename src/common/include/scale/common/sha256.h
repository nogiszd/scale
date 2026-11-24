#pragma once

// Portable SHA-256 (FIPS 180-4). Used for report subject identity.

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"

#include <string>

namespace scale {

// Lowercase hex digest (64 characters). Never fails today; Result is kept for API stability.
[[nodiscard]] Result<std::string> sha256_hex(ByteView data);

}  // namespace scale
