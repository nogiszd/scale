; test_seh.exe entry (T-301). See seh.cpp for the contract.
;
; test_entry has unwind data that names test_seh_handler as its exception
; handler (PROC FRAME:handler sets UNW_FLAG_EHANDLER and UNW_FLAG_UHANDLER).
; It reads through R10 = 0. The handler is only found when the image's
; exception directory is registered with the OS dispatcher. It then points
; R10 at test_seh_value and resumes at the same instruction.

EXTERN test_seh_handler:PROC
EXTERN test_seh_value:DWORD
EXTERN test_seh_handled:DWORD

.code

test_entry PROC FRAME:test_seh_handler
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    xor r10, r10
    mov eax, dword ptr [r10]          ; faults once; retried with R10 = &test_seh_value
    add eax, test_seh_handled          ; + number of handled exceptions (expected 1)
    add rsp, 28h
    ret
test_entry ENDP

END
