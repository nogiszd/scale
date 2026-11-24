#include "scale/common/sha256.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {
std::string hash_of(std::string_view text) {
    const auto r = scale::sha256_hex(scale::ByteView(std::as_bytes(std::span(text.data(), text.size()))));
    EXPECT_TRUE(r.has_value());
    return r ? *r : std::string();
}
}  // namespace

// FIPS 180-2 / NIST test vectors.
TEST(Sha256, KnownVectors) {
    EXPECT_EQ(hash_of(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(hash_of("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(hash_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256, MillionAs) {
    const std::string text(1'000'000, 'a');
    EXPECT_EQ(hash_of(text), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
