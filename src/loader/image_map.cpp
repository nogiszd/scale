#include "scale/loader/image_map.h"

#include <windows.h>

#include <cstring>
#include <format>
#include <utility>

namespace scale::loader {
namespace {

constexpr std::uint16_t kRelocsStripped = 0x0001;
constexpr std::uint32_t kPageSize = 4096;

std::unexpected<Error> map_error(std::string_view code, std::string message) {
    return fail("loader", code, std::move(message));
}

std::uint32_t section_extent(const Section& s) {
    return s.virtual_size != 0 ? s.virtual_size : s.raw_size;
}

std::uint64_t align_up(std::uint64_t v, std::uint64_t a) {
    return (v + a - 1) / a * a;
}

DWORD protection_for(std::uint32_t characteristics) {
    const bool r = (characteristics & kSectionMemRead) != 0;
    const bool w = (characteristics & kSectionMemWrite) != 0;
    const bool x = (characteristics & kSectionMemExecute) != 0;
    if (x) return r ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if (w) return PAGE_READWRITE;
    if (r) return PAGE_READONLY;
    return PAGE_NOACCESS;
}

// Rejects everything outside the supported subset before allocating memory.
Result<void> check_supported(const PeImage& image) {
    if (image.is_dll()) {
        return map_error("unsupported_image", "DLL images are not supported by the loader yet");
    }
    if (image.section_alignment < kPageSize) {
        return map_error("unsupported_image",
                         std::format("SectionAlignment 0x{:x} is below the page size", image.section_alignment));
    }
    for (std::size_t i = 0; i < kDirectoryCount; ++i) {
        const auto index = static_cast<DirectoryIndex>(i);
        if (!image.directories[i].present() || index == DirectoryIndex::base_relocation_table ||
            index == DirectoryIndex::debug) {
            continue;
        }
        return map_error("unsupported_directory",
                         std::format("the {} directory is not supported by the loader yet", directory_name(index)));
    }
    if (image.relocations) {
        for (unsigned type = 0; type < image.relocations->count_by_type.size(); ++type) {
            if (type != 0 && type != 10 && image.relocations->count_by_type[type] != 0) {
                return map_error("unsupported_relocation",
                                 std::format("base relocation type {} is not supported (only DIR64)", type));
            }
        }
    }
    const Section* entry_section = nullptr;
    for (const Section& s : image.sections) {
        const bool w = (s.characteristics & kSectionMemWrite) != 0;
        const bool x = (s.characteristics & kSectionMemExecute) != 0;
        if (w && x) {
            return map_error("writable_executable_section",
                             std::format("section '{}' is both writable and executable", s.name));
        }
        if (image.address_of_entry_point >= s.virtual_address &&
            image.address_of_entry_point - s.virtual_address < section_extent(s)) {
            entry_section = &s;
        }
    }
    if (image.address_of_entry_point == 0 || entry_section == nullptr ||
        (entry_section->characteristics & kSectionMemExecute) == 0) {
        return map_error("entry_not_executable",
                         std::format("entry point RVA 0x{:x} is not inside an executable section",
                                     image.address_of_entry_point));
    }
    return {};
}

}  // namespace

MappedImage::MappedImage(MappedImage&& other) noexcept
    : base_(std::exchange(other.base_, 0)),
      size_(std::exchange(other.size_, 0)),
      entry_(std::exchange(other.entry_, 0)),
      delta_(std::exchange(other.delta_, 0)),
      relocations_applied_(std::exchange(other.relocations_applied_, 0)) {}

MappedImage& MappedImage::operator=(MappedImage&& other) noexcept {
    if (this != &other) {
        release();
        base_ = std::exchange(other.base_, 0);
        size_ = std::exchange(other.size_, 0);
        entry_ = std::exchange(other.entry_, 0);
        delta_ = std::exchange(other.delta_, 0);
        relocations_applied_ = std::exchange(other.relocations_applied_, 0);
    }
    return *this;
}

MappedImage::~MappedImage() {
    release();
}

void MappedImage::release() noexcept {
    if (base_ != 0) {
        ::VirtualFree(reinterpret_cast<void*>(base_), 0, MEM_RELEASE);
        base_ = 0;
    }
}

Result<MappedImage> map_image(ByteView file, const PeImage& image) {
    if (auto r = check_supported(image); !r) {
        return std::unexpected(r.error());
    }
    const bool relocatable = image.relocations.has_value() && (image.characteristics & kRelocsStripped) == 0;

    // ---- Reserve and commit (read/write while populating) -------------------------
    void* preferred = relocatable ? nullptr : reinterpret_cast<void*>(image.image_base);
    void* mem = ::VirtualAlloc(preferred, image.size_of_image, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mem == nullptr) {
        if (!relocatable) {
            return map_error("relocation_required",
                             std::format("image has no relocations and its preferred base 0x{:x} is unavailable "
                                         "(Win32 error {})",
                                         image.image_base, ::GetLastError()));
        }
        return map_error("map_failed", std::format("VirtualAlloc of 0x{:x} bytes failed (Win32 error {})",
                                                   image.size_of_image, ::GetLastError()));
    }
    MappedImage mapped;
    mapped.base_ = reinterpret_cast<std::uintptr_t>(mem);
    mapped.size_ = image.size_of_image;
    auto* base = static_cast<std::byte*>(mem);

    // ---- Copy headers and section data (bounds validated by inspect_pe) --------------
    std::memcpy(base, file.bytes().data(), image.size_of_headers);
    for (const Section& s : image.sections) {
        const std::uint32_t copy = std::min(s.raw_size, section_extent(s));
        if (copy != 0) {
            std::memcpy(base + s.virtual_address, file.bytes().data() + s.raw_offset, copy);
        }
    }

    // ---- Base relocations ----------------------------------------------------------------
    mapped.delta_ = static_cast<std::int64_t>(mapped.base_ - image.image_base);
    if (relocatable && mapped.delta_ != 0) {
        const DataDirectory& dir = image.directory(DirectoryIndex::base_relocation_table);
        const ByteView table(std::span<const std::byte>(base + dir.rva, dir.size));
        std::size_t offset = 0;
        while (offset < table.size()) {
            const std::uint32_t page = *table.u32(offset);
            const std::uint32_t block_size = *table.u32(offset + 4);
            for (std::size_t k = 0; k < (block_size - 8) / 2; ++k) {
                const std::uint16_t e = *table.u16(offset + 8 + k * 2);
                if ((e >> 12) != 10) {
                    continue;  // ABSOLUTE padding (other types were rejected)
                }
                std::byte* target = base + page + (e & 0x0FFFu);
                std::uint64_t value = 0;
                std::memcpy(&value, target, sizeof(value));
                value += static_cast<std::uint64_t>(mapped.delta_);
                std::memcpy(target, &value, sizeof(value));
                ++mapped.relocations_applied_;
            }
            offset += block_size;
        }
    }

    // ---- Page protections ------------------------------------------------------------------
    DWORD old = 0;
    if (!::VirtualProtect(base, image.size_of_image, PAGE_NOACCESS, &old) ||
        !::VirtualProtect(base, align_up(image.size_of_headers, image.section_alignment), PAGE_READONLY, &old)) {
        return map_error("map_failed", std::format("VirtualProtect (headers) failed (Win32 error {})", ::GetLastError()));
    }
    for (const Section& s : image.sections) {
        const std::uint32_t extent = section_extent(s);
        if (extent == 0) {
            continue;
        }
        if (!::VirtualProtect(base + s.virtual_address, align_up(extent, image.section_alignment),
                              protection_for(s.characteristics), &old)) {
            return map_error("map_failed", std::format("VirtualProtect ('{}') failed (Win32 error {})", s.name,
                                                       ::GetLastError()));
        }
    }
    ::FlushInstructionCache(::GetCurrentProcess(), base, image.size_of_image);

    mapped.entry_ = mapped.base_ + image.address_of_entry_point;
    return mapped;
}

}  // namespace scale::loader
