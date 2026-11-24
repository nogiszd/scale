#pragma once

// Bounds-checked, non-owning little-endian view over untrusted bytes.
//
// Every read takes an explicit offset and returns std::nullopt when the read
// would leave the view. There is no implicit cursor, so callers cannot
// accidentally continue after a failed read.

#include "scale/common/checked.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

namespace scale {

static_assert(std::endian::native == std::endian::little,
              "ByteView assumes a little-endian host (Windows x64)");

class ByteView {
public:
    constexpr ByteView() noexcept = default;
    constexpr explicit ByteView(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] constexpr std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return bytes_.empty(); }
    [[nodiscard]] constexpr std::span<const std::byte> bytes() const noexcept { return bytes_; }

    [[nodiscard]] constexpr bool contains(std::size_t offset, std::size_t length) const noexcept {
        return range_within<std::size_t>(offset, length, bytes_.size());
    }

    // Sub-view of [offset, offset + length); nullopt if out of range.
    [[nodiscard]] constexpr std::optional<ByteView> subview(std::size_t offset,
                                                            std::size_t length) const noexcept {
        if (!contains(offset, length)) {
            return std::nullopt;
        }
        return ByteView(bytes_.subspan(offset, length));
    }

    // Reads a trivially copyable little-endian value at `offset`.
    template <class T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] std::optional<T> read(std::size_t offset) const noexcept {
        if (!contains(offset, sizeof(T))) {
            return std::nullopt;
        }
        T value;
        std::memcpy(&value, bytes_.data() + offset, sizeof(T));
        return value;
    }

    [[nodiscard]] std::optional<std::uint8_t> u8(std::size_t offset) const noexcept {
        return read<std::uint8_t>(offset);
    }
    [[nodiscard]] std::optional<std::uint16_t> u16(std::size_t offset) const noexcept {
        return read<std::uint16_t>(offset);
    }
    [[nodiscard]] std::optional<std::uint32_t> u32(std::size_t offset) const noexcept {
        return read<std::uint32_t>(offset);
    }
    [[nodiscard]] std::optional<std::uint64_t> u64(std::size_t offset) const noexcept {
        return read<std::uint64_t>(offset);
    }

private:
    std::span<const std::byte> bytes_{};
};

}  // namespace scale
