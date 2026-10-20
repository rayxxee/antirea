/*
 * sensor_a_tracer.c — Windows x86_64 implementation of Sensor A
 *
 * Detects debugger/tracer presence using:
 *   1. Raw PEB read: BeingDebugged byte at GS:[0x60]+2
 *      (gs:[0x60] = TEB, TEB->ProcessEnvironmentBlock, PEB->BeingDebugged)
 *   2. Direct syscall stub to NtQueryInformationProcess with:
 *      - ProcessDebugPort (7)
 *      - ProcessDebugFlags (31 / 0x1F)
 *      - ProcessDebugObjectHandle (30 / 0x1E)
 *
 * DESIGN RULE: do NOT call NtQueryInformationProcess via the ntdll.dll export
 * (which is a hookable trampoline that Frida/minhook can intercept). Use a
 * direct syscall stub with the raw syscall number instead.
 *
 * Phase 0: STUB — returns clean value 0 unconditionally.
 *          Full implementation added in Phase 1.
 *
 * Clean value: 0
 */

#include "mirage/runtime.h"

uint32_t mirage_sensor_a_tracer(void)
{
    /* STUB: Phase 1 will implement:
     *   1. Inline asm: mov rax, gs:[0x60]       ; PEB pointer
     *                  movzx eax, byte [rax+2]   ; BeingDebugged
     *   2. Direct syscall stub for NtQueryInformationProcess
     *      (syscall number varies by Windows version — resolve via
     *       PEB->Ldr ntdll module base scan at runtime, not hardcoded)
     *   3. Query ProcessDebugPort, ProcessDebugFlags, ProcessDebugObjectHandle
     *   4. Return 0 iff all checks clean
     */
    return 0u;
}
