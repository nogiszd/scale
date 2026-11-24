#pragma once

#include "commands.h"

namespace scale::cli {

// scale run <trusted-fixture.exe> [--report <path>] [--timeout-ms <n>]
// (docs/contracts/run.md)
int cmd_run(Args args);

}  // namespace scale::cli
