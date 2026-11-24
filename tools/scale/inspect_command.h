#pragma once

#include "commands.h"

namespace scale::cli {

// scale inspect <file> [--json]  (docs/contracts/inspect.md)
int cmd_inspect(Args args);

}  // namespace scale::cli
