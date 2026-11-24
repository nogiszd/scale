#include "scale/common/sha256.h"

// Portable SHA-256, FIPS 180-4 section 6.2 (decision 0004). Verified by the
// FIPS 180-2 test vectors in tests/unit/common/sha256_test.cpp.

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <format>

namespace scale {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

class Sha256 {
public:
    void update(const std::byte* data, std::size_t size) {
        total_bytes_ += size;
        while (size > 0) {
            const std::size_t take = std::min(size, block_.size() - used_);
            for (std::size_t i = 0; i < take; ++i) block_[used_ + i] = static_cast<std::uint8_t>(data[i]);
            used_ += take;
            data += take;
            size -= take;
            if (used_ == block_.size()) {
                compress();
                used_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bit_length = total_bytes_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            while (used_ < 64) block_[used_++] = 0;
            compress();
            used_ = 0;
        }
        while (used_ < 56) block_[used_++] = 0;
        for (int i = 7; i >= 0; --i) block_[used_++] = static_cast<std::uint8_t>(bit_length >> (i * 8));
        compress();
        std::array<std::uint8_t, 32> digest{};
        for (std::size_t i = 0; i < 8; ++i) {
            for (std::size_t k = 0; k < 4; ++k) digest[i * 4 + k] = static_cast<std::uint8_t>(h_[i] >> (24 - 8 * k));
        }
        return digest;
    }

private:
    void compress() {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t t = 0; t < 16; ++t) {
            w[t] = (std::uint32_t{block_[t * 4]} << 24) | (std::uint32_t{block_[t * 4 + 1]} << 16) |
                   (std::uint32_t{block_[t * 4 + 2]} << 8) | std::uint32_t{block_[t * 4 + 3]};
        }
        for (std::size_t t = 16; t < 64; ++t) {
            const std::uint32_t s0 = std::rotr(w[t - 15], 7) ^ std::rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            const std::uint32_t s1 = std::rotr(w[t - 2], 17) ^ std::rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (std::size_t t = 0; t < 64; ++t) {
            const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + kRoundConstants[t] + w[t];
            const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += h;
    }

    std::array<std::uint32_t, 8> h_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> block_{};
    std::size_t used_ = 0;
    std::uint64_t total_bytes_ = 0;
};

}  // namespace

Result<std::string> sha256_hex(ByteView data) {
    Sha256 hasher;
    hasher.update(data.bytes().data(), data.size());
    const auto digest = hasher.finish();
    std::string hex;
    hex.reserve(64);
    for (std::uint8_t b : digest) hex += std::format("{:02x}", b);
    return hex;
}

}  // namespace scale
