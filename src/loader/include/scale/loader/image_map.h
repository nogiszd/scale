#pragma once

// Minimal native image mapping for trusted test executables (T-202).
// Decision record: docs/decisions/0002-native-image-loading.md
//
// This is NOT a general executable loader. It maps a PE32+ AMD64 executable
// into private memory of the current process, applies DIR64 base
// relocations, sets per-section page protections and returns the entry
// address. Everything outside this small subset is rejected before any
// memory is made executable:
//   - DLLs (IMAGE_FILE_DLL)
//   - any present data directory except base_relocation_table and debug
//     (imports, TLS, exceptions, load config, delay imports, CLR, ... are not
//     supported yet and are named in the error)
//   - relocation types other than ABSOLUTE and DIR64
//   - sections that are both writable and executable
//   - an entry point of 0 or outside an executable section
//   - an image without relocations that cannot be placed at its preferred base
//
// Images with relocations are always placed at a system-chosen address, so
// the relocation path runs on every load.

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"
#include "scale/loader/pe_image.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace scale::loader {

class MappedImage {
public:
    MappedImage() = default;
    MappedImage(const MappedImage&) = delete;
    MappedImage& operator=(const MappedImage&) = delete;
    MappedImage(MappedImage&& other) noexcept;
    MappedImage& operator=(MappedImage&& other) noexcept;
    ~MappedImage();  // releases the mapping

    [[nodiscard]] std::uintptr_t base() const noexcept { return base_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::uintptr_t entry_point() const noexcept { return entry_; }
    // Difference between the actual and preferred base (0 if not relocated).
    [[nodiscard]] std::int64_t relocation_delta() const noexcept { return delta_; }
    [[nodiscard]] std::uint32_t relocations_applied() const noexcept { return relocations_applied_; }

private:
    friend Result<MappedImage> map_image(ByteView, const PeImage&);
    void release() noexcept;

    std::uintptr_t base_ = 0;
    std::size_t size_ = 0;
    std::uintptr_t entry_ = 0;
    std::int64_t delta_ = 0;
    std::uint32_t relocations_applied_ = 0;
};

// Maps `file` (already validated as `image` by inspect_pe on the same bytes).
// Error codes (component "loader"): unsupported_image, unsupported_directory,
// unsupported_relocation, writable_executable_section, entry_not_executable,
// relocation_required, map_failed.
[[nodiscard]] Result<MappedImage> map_image(ByteView file, const PeImage& image);

// Reads a whole file read-only, rejecting files larger than `max_size`.
// Error codes: file_open_failed, file_read_failed, file_too_large.
[[nodiscard]] Result<std::vector<std::byte>> read_file(const std::filesystem::path& path,
                                                       std::uint64_t max_size = kDefaultMaxFileSize);

}  // namespace scale::loader
