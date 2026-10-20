/**
 * test_runtime_phase0.cpp — Phase 0 unit tests for libmirage_runtime
 *
 * Tests that the Phase 0 stubs return correct clean values.
 * These tests serve as the baseline that Phase 1 must not regress:
 *   - Sensor A clean value = 0
 *   - Sensor B clean value = 0x9E3779B9
 *   - Sensor C returns golden_hash when stub (= always clean)
 *   - FNV-1a: known-answer test
 *   - CRC32: known-answer test
 *   - mirage_env_poll: assembled correctly from sensors
 *
 * Platform: runs on both Linux and Windows — no platform-specific code here.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
#include "mirage/runtime.h"
}

/* Minimal test framework — no external deps in Phase 0 */
static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                       \
        auto _a = (a); auto _b = (b);                                         \
        if (_a == _b) {                                                        \
            ++g_passed;                                                        \
        } else {                                                               \
            ++g_failed;                                                        \
            std::fprintf(stderr, "FAIL %s:%d: %s == %s  got 0x%llx vs 0x%llx\n", \
                __FILE__, __LINE__, #a, #b,                                    \
                (unsigned long long)_a, (unsigned long long)_b);              \
        }                                                                      \
    } while(0)

#define EXPECT_NE(a, b)                                                       \
    do {                                                                       \
        auto _a = (a); auto _b = (b);                                         \
        if (_a != _b) { ++g_passed; }                                         \
        else {                                                                 \
            ++g_failed;                                                        \
            std::fprintf(stderr, "FAIL %s:%d: %s != %s  both = 0x%llx\n",    \
                __FILE__, __LINE__, #a, #b, (unsigned long long)_a);          \
        }                                                                      \
    } while(0)

int main()
{
    std::printf("=== AntiREA Phase 0 runtime unit tests ===\n");

    /* ── Sensor A: clean value = 0 ── */
    EXPECT_EQ(mirage_sensor_a_tracer(), 0u);

    /* ── Sensor B: clean value = 0x9E3779B9 ── */
    EXPECT_EQ(mirage_sensor_b_hook_scan(), 0x9E3779B9u);

    /* ── Sensor C stub: must return golden_hash ── */
    EXPECT_EQ(mirage_sensor_c_self_checksum(nullptr, 0, 0xDEADBEEFu), 0xDEADBEEFu);
    EXPECT_EQ(mirage_sensor_c_self_checksum(nullptr, 64, 0x12345678u), 0x12345678u);

    /* ── Sensor D stub: clean value = 0 ── */
    EXPECT_EQ(mirage_sensor_d_timing(1000), 0u);

    /* ── Combined poll: check s1/s2 clean values ── */
    MirageEnvStatus st = mirage_env_poll(0xCAFEBABEu, 1000u);
    EXPECT_EQ(st.s1, 0u);               /* Sensor A clean */
    EXPECT_EQ(st.s2, 0x9E3779B9u);      /* Sensor B clean */
    EXPECT_EQ(st.s3, 0xCAFEBABEu);      /* Sensor C stub returns golden */
    EXPECT_EQ(st.s4, 0u);               /* Sensor D clean */

    /* ── FNV-1a known-answer tests ── */
    /* FNV-1a("") = 0x811c9dc5 (the offset basis itself) */
    EXPECT_EQ(mirage_fnv1a32(nullptr, 0), 0x811c9dc5u);

    /* FNV-1a("a") = 0xe40c292c (reference value from FNV spec) */
    const uint8_t a_byte[] = { 'a' };
    EXPECT_EQ(mirage_fnv1a32(a_byte, 1), 0xe40c292cu);

    /* FNV-1a("foobar") = 0xbf9cf968 */
    const uint8_t foobar[] = { 'f','o','o','b','a','r' };
    EXPECT_EQ(mirage_fnv1a32(foobar, 6), 0xbf9cf968u);

    /* ── CRC32 known-answer test ── */
    /* CRC32("123456789") = 0xCBF43926 (standard check value) */
    const uint8_t digits[] = { '1','2','3','4','5','6','7','8','9' };
    EXPECT_EQ(mirage_crc32(digits, 9, 0u), 0xCBF43926u);

    /* ── XOR stream stub: verify it XORs with low byte of seed ── */
    const uint8_t input[] = { 0xAA, 0xBB, 0xCC };
    uint8_t output[3] = {};
    mirage_xor_stream(0x0000000000000042ULL, input, output, 3);
    EXPECT_EQ(output[0], (uint8_t)(0xAA ^ 0x42));
    EXPECT_EQ(output[1], (uint8_t)(0xBB ^ 0x42));
    EXPECT_EQ(output[2], (uint8_t)(0xCC ^ 0x42));

    /* ── Sensor D calibrate stub ── */
    uint64_t max_cycles = 0;
    mirage_sensor_d_calibrate(&max_cycles);
    EXPECT_NE(max_cycles, 0u); /* must set something non-zero */

    /* ── Report ── */
    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
