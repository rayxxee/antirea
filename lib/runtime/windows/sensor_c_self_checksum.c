/*
 * sensor_c_self_checksum.c — Windows x86_64 implementation of Sensor C
 *
 * Same FNV-1a/CRC32 rolling-window logic as the Linux implementation.
 * Runtime base for ASLR-relative addressing obtained from the PEB module
 * list (not from a hardcoded link-time address).
 *
 * Phase 0: Hash primitives fully implemented; sensor wrapper stubbed.
 *          Identical to Linux implementation for Phase 0.
 */

#include "mirage/runtime.h"
#include <stdint.h>
#include <stddef.h>

/* ── FNV-1a 32-bit (shared implementation — see linux/ for algorithm notes) */
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

uint32_t mirage_sensor_c_self_checksum(const void *fn_base, size_t window,
                                        uint32_t golden_hash)
{
    /* STUB: identical semantics to Linux stub — always returns golden_hash.
     * Phase 1 will add the actual window read.
     * Windows specifics: use VirtualQuery to confirm the page is readable
     * before hashing (avoid AV/EDR guard pages causing access violations). */
    (void)fn_base;
    (void)window;
    return golden_hash;
}
