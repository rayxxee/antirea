/**
 * MirageProfile.h — profile / configuration schema for antirea-opt
 *
 * Loaded from --profile=<json> and propagated to all passes via the
 * ANTIREA_PROFILE environment variable (set by antirea-opt before
 * invoking the pass pipeline).
 *
 * DESIGN RULE (§6.1): every pass must be disablable/tunable via this
 * profile without touching other passes or requiring a recompile.
 *
 * DESIGN RULE (§6.3): techniques tagged shelf_life="disposable" are
 * tier-2 modules that may break when agent harnesses update. Structural
 * tier-1 techniques (Passes 1 & 2) must never have a dependency on
 * tier-2 outputs.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mirage {

/* ── MBA rotation config (§6.2) ─────────────────────────────────────────── */
/**
 * MBARotationConfig — the MBA identity basis and multiplier choices.
 * Treated as rotation-schedule data, refreshed quarterly (§6.2).
 * NOT hardcoded in the pass source.
 */
struct MBARotationConfig {
    /* Multiplier a1 (must be odd so gcd(a1, 2^64)=1) */
    uint64_t a1 = 0x5851F42D4C957F2DULL; /* default: splitmix64 multiplier */
    /* Additive constant a0 */
    uint64_t a0 = 0x14057B7EF767814FULL;
    /* Index into the identity basis set [0..N-1] */
    uint32_t basis_index = 0;

    /* Inverse a1^-1 mod 2^64 (computed from a1 at load time) */
    uint64_t a1_inv = 0; /* populated by MirageProfile::loadOrDefault */
};

/* ── Sensor D calibration (§3, Sensor D) ────────────────────────────────── */
struct SensorDConfig {
    /* Upper bound of bare-metal rdtsc cycle range for this device class.
     * Established by mirage_sensor_d_calibrate() at first launch.
     * Stored in profile to avoid re-calibration on every run.
     * Treat as rotation-schedule data, not a constant. */
    uint64_t cycles_max = 1000;
    bool     enabled    = true;
};

/* ── Pass 4b decompiler strings (§6.3 — tier-2 / disposable) ────────────── */
struct HoneypotConfig {
    bool enabled             = true;
    bool decompiler_strings  = false; /* tier-2: off by default (§6.3) */
    /* shelf_life tag: "disposable" for 4b, "structural" for 4a */
    /* Used by antirea-arena to selectively re-evolve payloads */
};

/* ── Top-level profile ───────────────────────────────────────────────────── */
struct MirageProfile {
    /* ── Pass toggles ───────────────────────────────────────────────────── */
    bool pass1_ephemeral_str   = true;
    bool pass2_env_cff         = true;
    bool pass3_poly_mba        = true;
    bool pass4_honeypot        = false; /* off by default in balanced */
    bool pass5_dynamic_barrier = false; /* off by default in balanced */

    /* ── Sensor D ────────────────────────────────────────────────────────── */
    SensorDConfig sensor_d;

    /* ── MBA rotation ────────────────────────────────────────────────────── */
    MBARotationConfig mba;

    /* ── Honeypot sub-config ─────────────────────────────────────────────── */
    HoneypotConfig honeypot;

    /* ── Profile metadata ────────────────────────────────────────────────── */
    std::string profile_name = "balanced";
    std::string schema_version = "0.1.0";

    /* ── Loader ──────────────────────────────────────────────────────────── */
    /**
     * Load profile from JSON file at path.
     * If path is nullptr or the file cannot be read, returns the "balanced"
     * default profile so the build degrades gracefully rather than aborting.
     */
    static MirageProfile loadOrDefault(const char *json_path);

    /**
     * Serialize this profile to a JSON string (for antirea-opt --dump-profile).
     */
    std::string toJSON() const;

private:
    /* Compute a1_inv = modular inverse of a1 mod 2^64 via extended Euclid */
    static uint64_t modinv64(uint64_t a);
};

} /* namespace mirage */
