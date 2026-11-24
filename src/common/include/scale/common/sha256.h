#pragma once

// SHA-256 via Windows CNG (BCrypt). Used for report subject identity.

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"

#include <string>

namespace scale {

// Lowercase hex digest (64 characters). Error code: common/hash_failed.
[[nodiscard]] Result<std::string> sha256_hex(ByteView data);

}  // namespace scale
