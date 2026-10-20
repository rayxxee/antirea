/*
 * sensor_a_tracer.c — Linux x86_64 implementation of Sensor A
 *
 * Detects tracer/debugger presence by reading TracerPid from
 * /proc/self/status via a direct inline syscall (bypassing libc open/read
 * which are hookable via LD_PRELOAD / Frida GOT patching).
 *
 * Phase 0: STUB — returns clean value 0 unconditionally.
 *          Full implementation added in Phase 1.
 *
 * Clean value: 0 (no tracer present)
 */

#include "mirage/runtime.h"

uint32_t mirage_sensor_a_tracer(void)
{
    /* STUB: Phase 1 will implement:
     *   1. open("/proc/self/status") via raw syscall(SYS_openat)
     *   2. read() the file content via raw syscall(SYS_read)
     *   3. parse "TracerPid:" field
     *   4. close() via raw syscall(SYS_close)
     *   5. return TracerPid value (0 = clean)
     *
     * No libc, no glibc wrappers, no PLT entries visible in the GOT.
     */
    return 0u;
}
