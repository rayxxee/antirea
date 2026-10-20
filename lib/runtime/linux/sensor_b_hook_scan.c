/*
 * sensor_b_hook_scan.c — Linux x86_64 implementation of Sensor B
 *
 * Detects hook/instrumentation artifacts by:
 *   1. Scanning thread names via /proc/self/task/*/comm for known
 *      instrumentation strings ("frida", "gum-js-loop", "linjector")
 *   2. Scanning /proc/self/maps for known library names
 *   3. Checking first bytes of hot libc functions for 0xCC (int3) or
 *      0xE9 (near jmp) breakpoint/trampoline signatures
 *
 * All /proc reads done via direct inline syscalls.
 *
 * Phase 0: STUB — returns clean value 0x9E3779B9 unconditionally.
 *          Full implementation added in Phase 1.
 *
 * Clean value: 0x9E3779B9
 */

#include "mirage/runtime.h"

uint32_t mirage_sensor_b_hook_scan(void)
{
    /* STUB: Phase 1 will implement:
     *   1. readdir /proc/self/task via getdents64 syscall
     *   2. read comm files for each thread
     *   3. strcmp against known frida/instrumentation strings
     *   4. scan /proc/self/maps for frida-agent, linjector, etc.
     *   5. mprotect-safe read of libc function prologues
     *   6. return 0x9E3779B9 if clean, other value if tampered
     */
    return 0x9E3779B9u;
}
