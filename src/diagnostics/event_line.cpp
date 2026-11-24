#include "scale/diagnostics/event_line.h"

#include <format>
#include <vector>

namespace scale::diagnostics {
namespace {

constexpr std::string_view kPrefix = "event ";

bool is_plain(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == ':' || c == '/' || c == '-';
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::optional<Severity> parse_severity(std::string_view s) {
    if (s == "info") return Severity::info;
    if (s == "warning") return Severity::warning;
    if (s == "error") return Severity::error;
    return std::nullopt;
}

}  // namespace

std::string percent_encode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (is_plain(c)) {
            out += ch;
        } else {
            out += std::format("%{:02X}", c);
        }
    }
    return out;
}

std::optional<std::string> percent_decode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch != '%') {
            if (!is_plain(static_cast<unsigned char>(ch))) return std::nullopt;
            out += ch;
            continue;
        }
        if (i + 2 >= value.size()) return std::nullopt;  // needs two hex digits
        const int hi = hex_digit(value[i + 1]);
        const int lo = hex_digit(value[i + 2]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out += static_cast<char>((hi << 4) | lo);
        i += 2;
    }
    return out;
}

std::string to_event_line(const DiagnosticEvent& e) {
    std::string line(kPrefix);
    line += std::format("component={} stage={} code={} severity={} fatal={}", percent_encode(e.component),
                        percent_encode(e.stage), percent_encode(e.code), to_string(e.severity), e.fatal ? 1 : 0);
    for (const Attribute& a : e.attributes) {
        line += std::format(" a.{}={}", percent_encode(a.key), percent_encode(a.value));
    }
    line += " message=" + percent_encode(e.message);
    return line;
}

std::optional<DiagnosticEvent> parse_event_line(std::string_view line) {
    if (!line.starts_with(kPrefix)) return std::nullopt;
    line.remove_prefix(kPrefix.size());

    std::vector<std::string_view> tokens;
    while (!line.empty()) {
        const auto space = line.find(' ');
        tokens.push_back(line.substr(0, space));
        if (space == std::string_view::npos) break;
        line.remove_prefix(space + 1);
    }
    // Fixed fields in order, then attributes, then message.
    constexpr std::string_view kFixed[] = {"component", "stage", "code", "severity", "fatal"};
    if (tokens.size() < 6) return std::nullopt;

    DiagnosticEvent e;
    std::string fixed[5];
    for (std::size_t i = 0; i < 5; ++i) {
        const auto eq = tokens[i].find('=');
        if (eq == std::string_view::npos || tokens[i].substr(0, eq) != kFixed[i]) return std::nullopt;
        auto value = percent_decode(tokens[i].substr(eq + 1));
        if (!value) return std::nullopt;
        fixed[i] = std::move(*value);
    }
    e.component = std::move(fixed[0]);
    e.stage = std::move(fixed[1]);
    e.code = std::move(fixed[2]);
    const auto severity = parse_severity(fixed[3]);
    if (!severity || (fixed[4] != "0" && fixed[4] != "1")) return std::nullopt;
    e.severity = *severity;
    e.fatal = fixed[4] == "1";
    if (e.component.empty() || e.stage.empty() || e.code.empty()) return std::nullopt;

    for (std::size_t i = 5; i + 1 < tokens.size(); ++i) {
        const std::string_view t = tokens[i];
        const auto eq = t.find('=');
        if (!t.starts_with("a.") || eq == std::string_view::npos || eq <= 2) return std::nullopt;
        auto key = percent_decode(t.substr(2, eq - 2));
        auto value = percent_decode(t.substr(eq + 1));
        if (!key || !value || key->empty()) return std::nullopt;
        e.attributes.push_back(Attribute{std::move(*key), std::move(*value)});
    }
    const std::string_view last = tokens.back();
    if (!last.starts_with("message=")) return std::nullopt;
    auto message = percent_decode(last.substr(8));
    if (!message) return std::nullopt;
    e.message = std::move(*message);
    return e;
}

}  // namespace scale::diagnostics
