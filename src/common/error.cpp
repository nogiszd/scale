#include "scale/common/error.h"

namespace scale {

std::string Error::to_string() const {
    std::string out;
    out.reserve(component.size() + code.size() + message.size() + 3);
    out += component;
    out += '/';
    out += code;
    if (!message.empty()) {
        out += ": ";
        out += message;
    }
    return out;
}

}  // namespace scale
