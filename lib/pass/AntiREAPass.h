/**
 * AntiREAPass.h — out-of-tree LLVM pass plugin public interface
 *
 * Five independently-toggleable passes, run in order at OptimizerLastEPCallback
 * so they execute after LLVM's own optimizer (InstCombine, SCCP, SimplifyCFG,
 * GVN) but before backend codegen.
 *
 * Hook point rationale (§2, Yates 2026):
 *   Running before the optimizer lets the optimizer strip naive constructs.
 *   Running at OptimizerLastEP means the optimizer has already run — our
 *   obfuscation is injected into the final IR that goes to codegen.
 *
 * Pass pipeline order (must be preserved — Pass 2 consumes Pass 1 output):
 *   1. EphemeralStrPass       (-antirea-ephemeral-str)
 *   2. EnvCFFPass             (-antirea-env-cff)
 *   3. PolyMBAPass            (-antirea-poly-mba)
 *   4. HoneypotPass           (-antirea-honeypot)
 *   5. DynamicBarrierPass     (-antirea-dynamic-barrier)
 */

#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"

namespace mirage {

/* ── Pass 1 ─────────────────────────────────────────────────────────────── */
/**
 * EphemeralStrPass — stack-scoped string hydration with volatile wipe.
 *
 * For every string literal in the function:
 *   1. Encrypt into .rodata at compile time (ChaCha20 or XOR stream)
 *   2. alloca a stack buffer immediately before the use site
 *   3. Inject inline decrypt loop
 *   4. On every return/unwind edge: inject llvm.memset with isVolatile=true
 *
 * INVARIANT enforced by VolatileWipeVerifier sub-pass:
 *   Any injected memset that lacks isVolatile=true is a build error.
 */
struct EphemeralStrPass : llvm::PassInfoMixin<EphemeralStrPass> {
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM);
    static bool isRequired() { return true; }
};

/* ── Pass 2 ─────────────────────────────────────────────────────────────── */
/**
 * EnvCFFPass — branchless environment-entangled control-flow flattening.
 *
 * Dispatcher state formula (§3):
 *   Δ_tamper   = (s1 != 0) OR (s2 != 0x9E3779B9)
 *   state_next = ID(B_real) XOR (sext(Δ_tamper) AND (ID(B_real) XOR ID(B_decoy)))
 *
 * sext(Δ_tamper): LLVM CreateSExt i1→i32 → 0x00000000 (clean) or 0xFFFFFFFF.
 * Zero conditional branches emitted. Decoy block B_decoy is not a dead end —
 * it produces a functionally consumed downstream value (§3, design requirement).
 */
struct EnvCFFPass : llvm::PassInfoMixin<EnvCFFPass> {
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM);
    static bool isRequired() { return true; }
};

/* ── Pass 3 ─────────────────────────────────────────────────────────────── */
/**
 * PolyMBAPass — polynomial mixed boolean-arithmetic obfuscation.
 *
 * Transforms sub-expressions E(x,y) using:
 *   Ẽ(x,y) = a1^-1 · (a1 · E_MBA(x,y) + a0) − a1^-1·a0   (mod 2^64)
 *
 * Opaque barrier: multiplies a zero-identity term by sensor s1, loaded
 * through asm volatile("" : "+r"(s1)) so the compiler cannot prove s1=0.
 *
 * Basis and multiplier choices are CONFIG-DRIVEN (not hardcoded) per §6.
 * See MBAConfig loaded from --profile JSON.
 */
struct PolyMBAPass : llvm::PassInfoMixin<PolyMBAPass> {
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM);
    static bool isRequired() { return false; } /* toggleable */
};

/* ── Pass 4 ─────────────────────────────────────────────────────────────── */
/**
 * HoneypotPass — effort-gated honeypots + decompiler-directed strings.
 *
 * 4a: Clone protected function signature → decoy with solvable cipher
 *     (4-round Feistel or RC4 with hardcoded seed) fronting a fake secret.
 * 4b: Inject decompiler-annotation strings (tier-2, independently toggleable
 *     via profile.honeypot.decompiler_strings: bool).
 *
 * 4b is tagged as shelf-life=disposable in the profile schema per §6.3.
 * No structural dependency from Pass 1/2 on Pass 4b.
 */
struct HoneypotPass : llvm::PassInfoMixin<HoneypotPass> {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &MAM);
    static bool isRequired() { return false; }
};

/* ── Pass 5 ─────────────────────────────────────────────────────────────── */
/**
 * DynamicBarrierPass — interrupt-on-dynamic-probe mid-function barrier.
 *
 * Thin coordination layer over Sensors A–D + step-cadence heuristic.
 * Triggers the same branchless decoy routing as Pass 2 mid-function,
 * not only at entry, to catch post-entry dynamic attachment.
 */
struct DynamicBarrierPass : llvm::PassInfoMixin<DynamicBarrierPass> {
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM);
    static bool isRequired() { return false; }
};

/* ── Verifier sub-pass ───────────────────────────────────────────────────── */
/**
 * VolatileWipeVerifier — ensures every injected memset has isVolatile=true.
 *
 * Fails the build (returns PreservedAnalyses::none() + emits error) if any
 * memset injected on an antirea-managed stack buffer lacks the volatile flag.
 * Without this, DSEPass (Dead Store Elimination) will silently delete the
 * wipe, reintroducing the lazy-decrypt flaw from §1.1 that Pass 1 fixes.
 *
 * Run as the final step after all five passes, always enabled.
 */
struct VolatileWipeVerifier : llvm::PassInfoMixin<VolatileWipeVerifier> {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &MAM);
    static bool isRequired() { return true; }
};

/* ── Registration helper ─────────────────────────────────────────────────── */
/**
 * registerAntiREAPasses — called from the plugin entry point.
 * Registers all passes with the LLVM PassBuilder at OptimizerLastEPCallback.
 */
void registerAntiREAPasses(llvm::PassBuilder &PB);

} /* namespace mirage */
