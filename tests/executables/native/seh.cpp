// test_seh.exe — x64 exception dispatch through the image's exception
// directory (T-301).
//
// Entry contract (custom test executable entry, NOT a CRT entry point):
//   int test_entry(void)   — seh_entry.asm, the image's AddressOfEntryPoint
// test_entry reads through a null R10. Its unwind data names
// test_seh_handler. The handler accepts only that access violation: it sets
// R10 to &test_seh_value (100), counts the exception and returns
// ExceptionContinueExecution, so the read is retried at the same RIP. The
// entry returns test_seh_value + test_seh_handled = 101.
// If the exception directory is not registered, the dispatcher finds no
// handler and the exception is unhandled (0xC0000005).
// Dependencies: none (no CRT, no imports). Windows headers only for the
// documented handler types (winnt.h); no API is called.
// Native reference: Windows registers .pdata itself; the exit code must be 101.

#include <windows.h>

extern "C" {
volatile LONG test_seh_value = 100;
volatile LONG test_seh_handled = 0;

EXCEPTION_DISPOSITION test_seh_handler(PEXCEPTION_RECORD record, PVOID, PCONTEXT context, PVOID) {
    // Unwind calls (EXCEPTION_UNWINDING) and anything else are not ours.
    if ((record->ExceptionFlags & EXCEPTION_UNWINDING) != 0 ||
        record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || context->R10 != 0 || test_seh_handled != 0) {
        return ExceptionContinueSearch;
    }
    context->R10 = reinterpret_cast<DWORD64>(&test_seh_value);
    test_seh_handled = test_seh_handled + 1;
    return ExceptionContinueExecution;
}
}
