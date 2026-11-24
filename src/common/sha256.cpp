#include "scale/common/sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <climits>
#include <format>

namespace scale {

Result<std::string> sha256_hex(ByteView data) {
    std::array<UCHAR, 32> digest{};
    // BCryptHash takes a ULONG length; hash in chunks via an incremental handle.
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status = ::BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return fail("common", "hash_failed", std::format("BCryptCreateHash failed (0x{:08x})",
                                                         static_cast<unsigned long>(status)));
    }
    const auto bytes = data.bytes();
    std::size_t offset = 0;
    while (BCRYPT_SUCCESS(status) && offset < bytes.size()) {
        const std::size_t chunk = std::min<std::size_t>(bytes.size() - offset, ULONG_MAX);
        status = ::BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data() + offset)),
                                  static_cast<ULONG>(chunk), 0);
        offset += chunk;
    }
    if (BCRYPT_SUCCESS(status)) {
        status = ::BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    }
    ::BCryptDestroyHash(hash);
    if (!BCRYPT_SUCCESS(status)) {
        return fail("common", "hash_failed", std::format("SHA-256 failed (0x{:08x})", static_cast<unsigned long>(status)));
    }
    std::string hex;
    hex.reserve(64);
    for (UCHAR b : digest) {
        hex += std::format("{:02x}", b);
    }
    return hex;
}

}  // namespace scale
