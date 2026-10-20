/**
 * mirage/runtime.h — libmirage_runtime public interface
 *
 * Single header covering all sensor and crypto primitives.
 * Implemented behind platform-specific compilation units in
 *   lib/runtime/linux/   (inline-syscall, Linux x86_64)
 *   lib/runtime/windows/ (PEB walk + direct-syscall stub, Windows x86_64)
 *
 * DESIGN RULE: nothing in this header or its implementations may call into
 * libc (Linux) or ntdll.dll exports (Windows) that a hooking framework could
 * intercept.  All kernel access goes through inline assembly syscall stubs.
 *
 * Phase 0: interface declarations only — implementations stubbed and added
 *          in Phase 1 (Runtime library).
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sensor result type ───────────────────────────────────────────────────── */

/**
 * MirageEnvStatus — packed result from a combined sensor poll.
 *
 * Fields:
 *   s1  : Sensor A result. Clean value = 0.
 *         Non-zero → tracer/debugger detected.
 *   s2  : Sensor B result. Clean value = 0x9E3779B9.
 *         Any other value → hook/instrumentation artifact detected.
 *   s3  : Sensor C result. Clean value = build-time golden hash.
 *         Mismatch → self-checksum failure (code tampered).
 *   s4  : Sensor D result. 0 = timing within bare-metal range.
 *         Non-zero → emulation/VM timing anomaly detected.
 */
typedef struct {
    uint32_t s1; /* Sensor A: tracer presence            (clean = 0)          */
    uint32_t s2; /* Sensor B: hook/instrumentation scan  (clean = 0x9E3779B9) */
    uint32_t s3; /* Sensor C: self-checksum              (clean = golden hash) */
    uint32_t s4; /* Sensor D: timing-delta VM detection  (clean = 0)          */
} MirageEnvStatus;

/* ── Sensor A — Tracer presence ──────────────────────────────────────────── */
/* Linux:   reads TracerPid from /proc/self/status via direct syscall.       */
/* Windows: NtQueryInformationProcess (direct stub) + PEB BeingDebugged.     */
uint32_t mirage_sensor_a_tracer(void);

/* ── Sensor B — Hook / instrumentation artifact scan ─────────────────────── */
/* Linux:   scans /proc/self/maps and thread names; checks libc prologues.   */
/* Windows: walks PEB Ldr list; checks ntdll/kernel32 prologues for 0xCC/E9. */
uint32_t mirage_sensor_b_hook_scan(void);

/* ── Sensor C — Self-checksum ─────────────────────────────────────────────── */
/* Rolling FNV-1a over a 64-byte window of the current function's code,      */
/* computed relative to the function's runtime base (ASLR-safe).             */
/* golden_hash: build-time expected value injected by the pass.              */
uint32_t mirage_sensor_c_self_checksum(const void *fn_base, size_t window,
                                        uint32_t golden_hash);

/* ── Sensor D — Timing-delta VM/emulation detection ─────────────────────── */
/* rdtsc-based. Linux: rdtsc only. Windows: rdtsc + QPC cross-check.        */
/* calibration_cycles_max: upper bound of bare-metal range for this device.  */
/* Returns 0 if within range, non-zero if emulation timing anomaly detected. */
uint32_t mirage_sensor_d_timing(uint64_t calibration_cycles_max);

/* ── Combined poll ────────────────────────────────────────────────────────── */
/* Runs all four sensors and returns packed results. Used by Pass 2's        */
/* branchless dispatcher.                                                     */
MirageEnvStatus mirage_env_poll(uint32_t sensor_c_golden,
                                 uint64_t sensor_d_cycles_max);

/* ── Crypto primitives ────────────────────────────────────────────────────── */

/**
 * ChaCha20 stream cipher — used by Pass 1 for string encryption.
 * key:    32-byte key
 * nonce:  12-byte nonce
 * input:  plaintext/ciphertext
 * output: ciphertext/plaintext (XOR stream)
 * len:    byte count
 */
void mirage_chacha20(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter,
                      const uint8_t *input, uint8_t *output, size_t len);

/**
 * Rolling split-key XOR — lightweight alternative to ChaCha20 for short
 * strings where ChaCha20 overhead dominates. Keyed by seed XOR env_checksum.
 */
void mirage_xor_stream(uint64_t seed, const uint8_t *input, uint8_t *output,
                        size_t len);

/* ── Hash primitives ─────────────────────────────────────────────────────── */

/** FNV-1a 32-bit hash. Used by Sensor C rolling window. */
uint32_t mirage_fnv1a32(const uint8_t *data, size_t len);

/** CRC32 rolling hash. Alternative for Sensor C. */
uint32_t mirage_crc32(const uint8_t *data, size_t len, uint32_t init);

/* ── Sensor D calibration ────────────────────────────────────────────────── */
/**
 * Run at first launch to establish the bare-metal baseline for this device.
 * Stores result in *out_cycles_max (suggested upper bound = mean + 3*stddev).
 * Call once, persist the result in your profile JSON for subsequent runs.
 */
void mirage_sensor_d_calibrate(uint64_t *out_cycles_max);

#ifdef __cplusplus
} /* extern "C" */
#endif
