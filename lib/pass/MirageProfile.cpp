/**
 * MirageProfile.cpp — profile JSON loader / serializer
 *
 * Uses nlohmann/json (vendored, header-only) for parsing.
 * Designed so that a missing key falls back to the default value —
 * partial profiles ("only override MBA rotation") work correctly.
 */

#include "MirageProfile.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>

using json = nlohmann::json;

namespace mirage {

/* ── Modular inverse mod 2^64 (extended binary GCD) ─────────────────────── */
uint64_t MirageProfile::modinv64(uint64_t a)
{
    /* a must be odd (gcd(a, 2^64) = 1). Verify in loadOrDefault. */
    uint64_t x = 1;
    for (int i = 0; i < 63; i++) {
        x *= x * a; /* Newton's method: x ← x*(2 - a*x) mod 2^64 */
        /* Actually use: x = x * (2 - a * x) */
    }
    /* Proper Newton iteration for mod-2^64 inverse */
    /* Seed: x = a (works for a ≡ 1 mod 2, i.e. a odd) */
    x = a;
    for (int i = 0; i < 6; i++) {       /* 6 iterations sufficient for 64-bit */
        x = x * (2 - a * x);
    }
    return x;
}

/* ── JSON helpers ────────────────────────────────────────────────────────── */
template<typename T>
static T jget(const json &j, const char *key, T def)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    try { return it->get<T>(); } catch (...) { return def; }
}

/* ── Loader ──────────────────────────────────────────────────────────────── */
MirageProfile MirageProfile::loadOrDefault(const char *json_path)
{
    MirageProfile p; /* starts with balanced defaults */

    if (!json_path || json_path[0] == '\0') {
        /* No profile specified — use balanced defaults */
        p.mba.a1_inv = modinv64(p.mba.a1);
        return p;
    }

    std::ifstream f(json_path);
    if (!f.is_open()) {
        std::fprintf(stderr,
            "[antirea] WARNING: cannot open profile '%s', using balanced defaults\n",
            json_path);
        p.mba.a1_inv = modinv64(p.mba.a1);
        return p;
    }

    json j;
    try {
        f >> j;
    } catch (const json::parse_error &e) {
        std::fprintf(stderr,
            "[antirea] WARNING: JSON parse error in '%s': %s — using balanced defaults\n",
            json_path, e.what());
        p.mba.a1_inv = modinv64(p.mba.a1);
        return p;
    }

    /* ── Profile metadata ── */
    p.profile_name    = jget<std::string>(j, "profile_name",    p.profile_name);
    p.schema_version  = jget<std::string>(j, "schema_version",  p.schema_version);

    /* ── Pass toggles ── */
    p.pass1_ephemeral_str   = jget<bool>(j, "pass1_ephemeral_str",   p.pass1_ephemeral_str);
    p.pass2_env_cff         = jget<bool>(j, "pass2_env_cff",         p.pass2_env_cff);
    p.pass3_poly_mba        = jget<bool>(j, "pass3_poly_mba",        p.pass3_poly_mba);
    p.pass4_honeypot        = jget<bool>(j, "pass4_honeypot",        p.pass4_honeypot);
    p.pass5_dynamic_barrier = jget<bool>(j, "pass5_dynamic_barrier", p.pass5_dynamic_barrier);

    /* ── Sensor D ── */
    if (j.contains("sensor_d") && j["sensor_d"].is_object()) {
        const auto &sd = j["sensor_d"];
        p.sensor_d.cycles_max = jget<uint64_t>(sd, "cycles_max", p.sensor_d.cycles_max);
        p.sensor_d.enabled    = jget<bool>    (sd, "enabled",    p.sensor_d.enabled);
    }

    /* ── MBA rotation ── */
    if (j.contains("mba") && j["mba"].is_object()) {
        const auto &mba = j["mba"];
        p.mba.a1          = jget<uint64_t>(mba, "a1",          p.mba.a1);
        p.mba.a0          = jget<uint64_t>(mba, "a0",          p.mba.a0);
        p.mba.basis_index = jget<uint32_t>(mba, "basis_index", p.mba.basis_index);

        /* Enforce a1 must be odd */
        if ((p.mba.a1 & 1) == 0) {
            std::fprintf(stderr,
                "[antirea] WARNING: mba.a1=0x%llx is even — must be odd for "
                "invertibility mod 2^64. Using default a1.\n",
                (unsigned long long)p.mba.a1);
            p.mba.a1 = 0x5851F42D4C957F2DULL;
        }
    }

    /* ── Honeypot sub-config ── */
    if (j.contains("honeypot") && j["honeypot"].is_object()) {
        const auto &hp = j["honeypot"];
        p.honeypot.enabled            = jget<bool>(hp, "enabled",            p.honeypot.enabled);
        p.honeypot.decompiler_strings = jget<bool>(hp, "decompiler_strings", p.honeypot.decompiler_strings);
    }

    p.mba.a1_inv = modinv64(p.mba.a1);
    return p;
}

/* ── Serializer ──────────────────────────────────────────────────────────── */
std::string MirageProfile::toJSON() const
{
    json j;
    j["schema_version"]        = schema_version;
    j["profile_name"]          = profile_name;
    j["pass1_ephemeral_str"]   = pass1_ephemeral_str;
    j["pass2_env_cff"]         = pass2_env_cff;
    j["pass3_poly_mba"]        = pass3_poly_mba;
    j["pass4_honeypot"]        = pass4_honeypot;
    j["pass5_dynamic_barrier"] = pass5_dynamic_barrier;

    j["sensor_d"] = {
        {"cycles_max", sensor_d.cycles_max},
        {"enabled",    sensor_d.enabled}
    };

    j["mba"] = {
        {"a1",          mba.a1},
        {"a0",          mba.a0},
        {"basis_index", mba.basis_index}
        /* a1_inv not serialized — recomputed at load time */
    };

    j["honeypot"] = {
        {"enabled",            honeypot.enabled},
        {"decompiler_strings", honeypot.decompiler_strings},
        {"_shelf_life_4b",     "disposable"}  /* §6.3 tag — informational */
    };

    return j.dump(2);
}

} /* namespace mirage */
