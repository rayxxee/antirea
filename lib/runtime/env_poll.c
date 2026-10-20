/*
 * env_poll.c — combined sensor poll (platform-agnostic aggregator)
 *
 * Calls all four sensors and packages the results into MirageEnvStatus.
 * This translation unit is shared between Linux and Windows builds —
 * it calls only mirage_sensor_* functions which are platform-specific.
 *
 * Sensor D (timing) and the ChaCha20/XOR/calibration stubs also live here
 * until Phase 1 separates them into dedicated files.
 */

#include "mirage/runtime.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── Combined poll ────────────────────────────────────────────────────────── */
MirageEnvStatus mirage_env_poll(uint32_t sensor_c_golden,
                                 uint64_t sensor_d_cycles_max)
{
    MirageEnvStatus st;
    st.s1 = mirage_sensor_a_tracer();
    st.s2 = mirage_sensor_b_hook_scan();
    /* Sensor C: fn_base=NULL and window=0 → stub returns golden_hash always */
    st.s3 = mirage_sensor_c_self_checksum(NULL, 0, sensor_c_golden);
    st.s4 = mirage_sensor_d_timing(sensor_d_cycles_max);
    return st;
}

/* ── Sensor D — Timing-delta VM/emulation detection ─────────────────────── */
/*
 * Phase 0 STUB: always returns 0 (clean).
 *
 * Phase 1 will implement:
 *   Linux:   rdtsc before/after a synthetic sensitive-instruction sequence;
 *            compare delta against calibration_cycles_max.
 *   Windows: same rdtsc approach + QueryPerformanceCounter cross-check
 *            (catches emulators that patch rdtsc but miss QPC).
 *
 * Bare-metal calibration rule of thumb (§3, Sensor D):
 *   ~150–300 cycles on bare metal; >1000 cycles in QEMU/Unicorn DBT.
 *   Treat the threshold as config data, not a constant — see §6.
 */
uint32_t mirage_sensor_d_timing(uint64_t calibration_cycles_max)
{
    (void)calibration_cycles_max;
    return 0u;
}

void mirage_sensor_d_calibrate(uint64_t *out_cycles_max)
{
    /* STUB: Phase 1 will run the rdtsc loop N times and compute mean+3σ. */
    if (out_cycles_max) {
        *out_cycles_max = 1000u; /* conservative default */
    }
}

/* ── ChaCha20 ─────────────────────────────────────────────────────────────── */
/*
 * Phase 0 STUB: copies input to output unmodified.
 * Phase 1 will implement the full ChaCha20 quarter-round / block function.
 * References: RFC 7539 (ChaCha20 and Poly1305).
 */
void mirage_chacha20(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter,
                      const uint8_t *input, uint8_t *output, size_t len)
{
    (void)key; (void)nonce; (void)counter;
    /* Identity transform — replace with real ChaCha20 in Phase 1 */
    if (input != output) {
        for (size_t i = 0; i < len; i++) {
            output[i] = input[i];
        }
    }
}

/* ── XOR stream ───────────────────────────────────────────────────────────── */
/*
 * Phase 0 STUB: XORs each byte with the low byte of seed.
 * Phase 1 will implement: C[i] = S[i] XOR PRNG(K_S XOR E_clean, i)
 * using a proper LCG or SFC seeded by (seed ^ env_checksum).
 */
void mirage_xor_stream(uint64_t seed, const uint8_t *input, uint8_t *output,
                        size_t len)
{
    uint8_t key_byte = (uint8_t)(seed & 0xFFu);
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key_byte;
    }
}
