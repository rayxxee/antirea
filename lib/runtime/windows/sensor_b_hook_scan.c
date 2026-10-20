/*
 * sensor_b_hook_scan.c — Windows x86_64 implementation of Sensor B
 *
 * Detects hook/instrumentation artifacts by:
 *   1. Walking the PEB Ldr module list directly (not via GetModuleHandle,
 *      which is itself hookable) for known instrumentation DLLs:
 *        frida-gadget.dll, frida-agent.dll, frida-helper-64.dll,
 *        minhook.dll, detours.dll
 *   2. Checking the first 5 bytes of hot ntdll/kernel32 exports for
 *      0xCC (int3 breakpoint) or 0xE9 (near jmp trampoline) patches.
 *
 * PEB walk path (x86_64):
 *   GS:[0x60]            → PEB
 *   PEB+0x18             → PEB_LDR_DATA*
 *   PEB_LDR_DATA+0x10    → InMemoryOrderModuleList (LIST_ENTRY)
 *   Walk forward checking BaseDllName.Buffer for each LDR_DATA_TABLE_ENTRY.
 *
 * Phase 0: STUB — returns clean value 0x9E3779B9 unconditionally.
 *          Full implementation added in Phase 1.
 *
 * Clean value: 0x9E3779B9
 */

#include "mirage/runtime.h"

uint32_t mirage_sensor_b_hook_scan(void)
{
    /* STUB: Phase 1 will implement PEB Ldr walk + prologue byte check. */
    return 0x9E3779B9u;
}
