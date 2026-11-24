#include "inspect_command.h"

#include "scale/common/text.h"
#include "scale/common/version.h"
#include "scale/diagnostics/json_writer.h"
#include "scale/loader/pe_image.h"

#include <format>
#include <optional>
#include <string>

namespace scale::cli {
namespace {

using diagnostics::JsonWriter;
using namespace loader;

std::string hex(std::uint64_t v) {
    return std::format("0x{:x}", v);
}

std::string hex4(std::uint32_t v) {
    return std::format("0x{:04x}", v);
}

std::string access(std::uint32_t characteristics) {
    std::string a;
    if (characteristics & kSectionMemRead) a += 'r';
    if (characteristics & kSectionMemWrite) a += 'w';
    if (characteristics & kSectionMemExecute) a += 'x';
    return a;
}

std::string relocation_type_name(unsigned type) {
    switch (type) {
        case 0: return "absolute";
        case 1: return "high";
        case 2: return "low";
        case 3: return "highlow";
        case 10: return "dir64";
        default: return std::format("type_{}", type);
    }
}

void write_tool(JsonWriter& w) {
    w.key("tool");
    w.begin_object();
    w.field("name", "scale");
    w.field("version", kVersionString);
    w.end_object();
}

std::string json_error(std::string_view file, const Error& e) {
    JsonWriter w;
    w.begin_object();
    w.field("schema_version", 1);
    write_tool(w);
    w.field("file", file);
    w.field("result", "error");
    w.key("error");
    w.begin_object();
    w.field("component", e.component);
    w.field("code", e.code);
    w.field("message", e.message);
    w.end_object();
    w.end_object();
    return w.str() + "\n";
}

std::string json_image(std::string_view file, const PeImage& img) {
    JsonWriter w;
    w.begin_object();
    w.field("schema_version", 1);
    write_tool(w);
    w.field("file", file);
    w.field("result", "ok");

    w.key("image");
    w.begin_object();
    w.field("machine", hex4(img.machine));
    w.field("characteristics", hex4(img.characteristics));
    w.field("is_dll", img.is_dll());
    w.field("time_date_stamp", std::format("0x{:08x}", img.time_date_stamp));
    w.field("linker_version", std::format("{}.{}", img.major_linker_version, img.minor_linker_version));
    w.field("entry_point_rva", hex(img.address_of_entry_point));
    w.field("image_base", hex(img.image_base));
    w.field("section_alignment", hex(img.section_alignment));
    w.field("file_alignment", hex(img.file_alignment));
    w.field("size_of_image", hex(img.size_of_image));
    w.field("size_of_headers", hex(img.size_of_headers));
    w.field("subsystem", img.subsystem);
    w.field("subsystem_version", std::format("{}.{}", img.major_subsystem_version, img.minor_subsystem_version));
    w.field("dll_characteristics", hex4(img.dll_characteristics));
    w.field("stack_reserve", hex(img.size_of_stack_reserve));
    w.field("stack_commit", hex(img.size_of_stack_commit));
    w.field("heap_reserve", hex(img.size_of_heap_reserve));
    w.field("heap_commit", hex(img.size_of_heap_commit));
    w.field("number_of_rva_and_sizes", img.number_of_rva_and_sizes);
    w.end_object();

    w.key("directories");
    w.begin_array();
    for (std::size_t i = 0; i < kDirectoryCount; ++i) {
        const DataDirectory& d = img.directories[i];
        if (!d.present()) continue;
        w.begin_object();
        w.field("name", directory_name(static_cast<DirectoryIndex>(i)));
        w.field("rva", hex(d.rva));
        w.field("size", hex(d.size));
        w.end_object();
    }
    w.end_array();

    w.key("sections");
    w.begin_array();
    for (const Section& s : img.sections) {
        w.begin_object();
        w.field("name", s.name);
        w.field("virtual_address", hex(s.virtual_address));
        w.field("virtual_size", hex(s.virtual_size));
        w.field("raw_offset", hex(s.raw_offset));
        w.field("raw_size", hex(s.raw_size));
        w.field("characteristics", std::format("0x{:08x}", s.characteristics));
        w.field("access", access(s.characteristics));
        w.end_object();
    }
    w.end_array();

    w.key("imports");
    w.begin_array();
    for (const ImportModule& m : img.imports) {
        w.begin_object();
        w.field("module", m.dll_name);
        w.field("lookup_table_rva", hex(m.import_lookup_table_rva));
        w.field("address_table_rva", hex(m.import_address_table_rva));
        w.key("entries");
        w.begin_array();
        for (const ImportEntry& e : m.entries) {
            w.begin_object();
            if (e.by_ordinal) {
                w.field("kind", "ordinal");
                w.field("ordinal", e.ordinal);
            } else {
                w.field("kind", "name");
                w.field("name", e.name);
                w.field("hint", e.hint);
            }
            w.end_object();
        }
        w.end_array();
        w.end_object();
    }
    w.end_array();

    w.key("relocations");
    if (img.relocations) {
        w.begin_object();
        w.field("blocks", img.relocations->block_count);
        w.field("entries", img.relocations->entry_count);
        w.key("by_type");
        w.begin_object();
        for (unsigned t = 0; t < img.relocations->count_by_type.size(); ++t) {
            if (img.relocations->count_by_type[t] != 0) {
                w.field(relocation_type_name(t), img.relocations->count_by_type[t]);
            }
        }
        w.end_object();
        w.end_object();
    } else {
        w.null();
    }

    w.key("tls");
    if (img.tls) {
        w.begin_object();
        w.field("callbacks_va", hex(img.tls->address_of_callbacks));
        w.key("callback_rvas");
        w.begin_array();
        for (std::uint32_t rva : img.tls->callback_rvas) {
            w.value(hex(rva));
        }
        w.end_array();
        w.field("size_of_zero_fill", img.tls->size_of_zero_fill);
        w.end_object();
    } else {
        w.null();
    }

    w.key("exceptions");
    if (img.exceptions) {
        w.begin_object();
        w.field("runtime_functions", img.exceptions->runtime_function_count);
        w.end_object();
    } else {
        w.null();
    }

    w.end_object();
    return w.str() + "\n";
}

std::string text_image(std::string_view file, const PeImage& img) {
    std::string t;
    t += std::format("file:            {}\n", file);
    t += std::format("type:            PE32+ AMD64 {}\n", img.is_dll() ? "DLL" : "executable");
    t += std::format("entry point RVA: {}\n", hex(img.address_of_entry_point));
    t += std::format("image base:      {}\n", hex(img.image_base));
    t += std::format("size of image:   {}\n", hex(img.size_of_image));
    t += std::format("subsystem:       {} (version {}.{})\n", img.subsystem, img.major_subsystem_version,
                     img.minor_subsystem_version);
    t += std::format("DLL flags:       {}\n", hex4(img.dll_characteristics));

    t += std::format("\nsections ({}):\n", img.sections.size());
    for (const Section& s : img.sections) {
        t += std::format("  {:<8} va {:<10} vsize {:<8} raw {}+{} {}\n", s.name, hex(s.virtual_address),
                         hex(s.virtual_size), hex(s.raw_offset), hex(s.raw_size), access(s.characteristics));
    }

    t += "\ndirectories present:";
    bool any = false;
    for (std::size_t i = 0; i < kDirectoryCount; ++i) {
        if (img.directories[i].present()) {
            t += std::format(" {}", directory_name(static_cast<DirectoryIndex>(i)));
            any = true;
        }
    }
    t += any ? "\n" : " none\n";

    t += std::format("\nimports ({} modules):\n", img.imports.size());
    for (const ImportModule& m : img.imports) {
        t += std::format("  {}\n", m.dll_name);
        for (const ImportEntry& e : m.entries) {
            if (e.by_ordinal) {
                t += std::format("    ordinal {}\n", e.ordinal);
            } else {
                t += std::format("    {} (hint {})\n", e.name, e.hint);
            }
        }
    }

    if (img.relocations) {
        t += std::format("\nrelocations: {} blocks, {} entries", img.relocations->block_count,
                         img.relocations->entry_count);
        for (unsigned ty = 0; ty < img.relocations->count_by_type.size(); ++ty) {
            if (img.relocations->count_by_type[ty] != 0) {
                t += std::format(", {} {}", relocation_type_name(ty), img.relocations->count_by_type[ty]);
            }
        }
        t += "\n";
    } else {
        t += "\nrelocations: none\n";
    }
    t += img.tls ? std::format("tls: present, {} callbacks\n", img.tls->callback_rvas.size()) : "tls: none\n";
    t += img.exceptions ? std::format("exceptions: {} runtime functions\n", img.exceptions->runtime_function_count)
                        : "exceptions: none\n";
    return t;
}

}  // namespace

int cmd_inspect(Args args) {
    bool json = false;
    std::optional<std::string> file;
    for (const std::string& a : args) {
        if (a == "--json") {
            json = true;
        } else if (a.starts_with("-")) {
            err(std::format("error: unknown option '{}'\nusage: scale inspect <file> [--json]\n", a));
            return kExitUsage;
        } else if (file) {
            err("error: 'inspect' takes exactly one file\nusage: scale inspect <file> [--json]\n");
            return kExitUsage;
        } else {
            file = a;
        }
    }
    if (!file) {
        err("usage: scale inspect <file> [--json]\n");
        return kExitUsage;
    }

    const auto wide = to_wide(*file);
    if (!wide) {
        err(std::format("error: {}\n", wide.error().to_string()));
        return kExitUsage;
    }
    const auto image = inspect_pe_file(std::filesystem::path(*wide));
    if (!image) {
        if (json) {
            out(json_error(*file, image.error()));
        }
        err(std::format("error: {}\n", image.error().to_string()));
        return kExitFailure;
    }
    out(json ? json_image(*file, *image) : text_image(*file, *image));
    return kExitSuccess;
}

}  // namespace scale::cli
