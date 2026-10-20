/*
 * sensor_c_self_checksum.c — Linux x86_64 implementation of Sensor C
 *
 * Rolling FNV-1a over a 64-byte window of the calling function's machine
 * code, computed relative to the function's runtime base address so that
 * ASLR/PIE relocation does not produce false positives.
 *
 * CRITICAL DESIGN NOTE (from spec §3, Sensor C):
 *   Must hash the *relative* byte window post-relocation (i.e. always
 *   read from fn_base + offset, not from a hardcoded link-time address).
 *   Hardcoding absolute addresses = false positives on every normal OS run.
 *
 * Phase 0: FNV-1a and CRC32 hash primitives fully implemented here.
 *          Sensor C wrapper is a STUB — returns golden_hash unconditionally
 *          (i.e. always "clean"). Full integration with Pass 2's dispatcher
 *          happens in Phase 3.
 *
 * Clean value: matches build-time golden_hash
 */

#include "mirage/runtime.h"
#include <stdint.h>
#include <stddef.h>

/* ── FNV-1a 32-bit ────────────────────────────────────────────────────────── */
#define FNV1A32_OFFSET 0x811c9dc5u
#define FNV1A32_PRIME  0x01000193u

uint32_t mirage_fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t hash = FNV1A32_OFFSET;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)data[i];
        hash *= FNV1A32_PRIME;
    }
    return hash;
}

/* ── CRC32 (table-free, bit-at-a-time for minimal rodata footprint) ──────── */
uint32_t mirage_crc32(const uint8_t *data, size_t len, uint32_t crc)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/* ── Sensor C wrapper ────────────────────────────────────────────────────── */
uint32_t mirage_sensor_c_self_checksum(const void *fn_base, size_t window,
                                        uint32_t golden_hash)
{
    /* STUB: Phase 1 will implement the actual rolling-window hash.
     *
     * Full implementation will:
     *   1. Cast fn_base to (const uint8_t *)
     *   2. Call mirage_fnv1a32(fn_base, window) — window capped at 64 bytes
     *   3. Return the computed hash for comparison against golden_hash
     *      by the Pass 2 dispatcher
     *
     * Returning golden_hash here means "always clean" during Phase 0 —
     * the dispatcher Δ_tamper will always be 0 (no false positives).
     */
    (void)fn_base;
    (void)window;
    return golden_hash;
}
