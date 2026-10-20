/**
 * AntiREAPass.cpp — out-of-tree LLVM pass plugin implementation
 *
 * Phase 0: Plugin registration skeleton + pass stubs.
 *          All passes are no-ops that return PreservedAnalyses::all().
 *          Implementations added phase-by-phase per §4 build plan.
 *
 * Plugin entry point: llvmGetPassPluginInfo() registered with
 *   OptimizerLastEPCallback — runs after all LLVM optimization passes so
 *   protections are not stripped by InstCombine/SCCP/SimplifyCFG/GVN.
 */

#include "AntiREAPass.h"
#include "MirageProfile.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace mirage;

/* ── Pass 1: EphemeralStrPass ───────────────────────────────────────────── */
PreservedAnalyses EphemeralStrPass::run(Function &F,
                                         FunctionAnalysisManager &FAM)
{
    /* STUB — Phase 2 will implement:
     *   1. Collect all GlobalVariable string literals used in F
     *   2. Per-string: generate random 64-bit seed K_S
     *   3. Encrypt with mirage_chacha20(K_S XOR E_clean) → store in .rodata
     *   4. Before each use site: alloca [L x i8], inject decrypt loop
     *   5. On every return/unwind edge: inject llvm.memset isVolatile=true
     */
    (void)F; (void)FAM;
    return PreservedAnalyses::all();
}

/* ── Pass 2: EnvCFFPass ─────────────────────────────────────────────────── */
PreservedAnalyses EnvCFFPass::run(Function &F,
                                   FunctionAnalysisManager &FAM)
{
    /* STUB — Phase 3 will implement:
     *   1. Build dispatcher switch over basic blocks
     *   2. For each B_i → B_real edge, synthesize B_decoy
     *   3. Compute: state_next = ID(B_real) XOR (sext(Δ_tamper) AND mask)
     *   4. Inject mirage_env_poll() call; use s1, s2 for Δ_tamper
     *   5. B_decoy must feed a downstream computation (not a dead end)
     */
    (void)F; (void)FAM;
    return PreservedAnalyses::all();
}

/* ── Pass 3: PolyMBAPass ────────────────────────────────────────────────── */
PreservedAnalyses PolyMBAPass::run(Function &F,
                                    FunctionAnalysisManager &FAM)
{
    /* STUB — Phase 4 will implement:
     *   1. Walk binary instructions (add, sub, and, or, xor)
     *   2. Apply linear-MBA identity from config-driven basis set
     *   3. Wrap with affine permutation a1^-1 · (a1 · E_MBA + a0) - a1^-1·a0
     *   4. Inject opaque barrier: asm volatile("" : "+r"(s1))
     *
     * Basis rotation: loaded from MirageProfile, not hardcoded (§6 req).
     */
    (void)F; (void)FAM;
    return PreservedAnalyses::all();
}

/* ── Pass 4: HoneypotPass ───────────────────────────────────────────────── */
PreservedAnalyses HoneypotPass::run(Module &M,
                                     ModuleAnalysisManager &MAM)
{
    /* STUB — Phase 5 will implement:
     *   4a: Clone protected function → decoy symbol (high-signal name)
     *       Embed fake secret behind 4-round Feistel or RC4
     *   4b: (if profile.honeypot.decompiler_strings == true)
     *       Inject dead-code annotation strings
     *
     * 4b is tier-2/disposable per §6.3 — no dependency from Pass 1/2.
     */
    (void)M; (void)MAM;
    return PreservedAnalyses::all();
}

/* ── Pass 5: DynamicBarrierPass ─────────────────────────────────────────── */
PreservedAnalyses DynamicBarrierPass::run(Function &F,
                                           FunctionAnalysisManager &FAM)
{
    /* STUB — Phase 5b will implement:
     *   Step-cadence heuristic + Sensor D at mid-function points
     *   Same branchless decoy routing as Pass 2 but triggered mid-function
     */
    (void)F; (void)FAM;
    return PreservedAnalyses::all();
}

/* ── VolatileWipeVerifier ────────────────────────────────────────────────── */
PreservedAnalyses VolatileWipeVerifier::run(Module &M,
                                             ModuleAnalysisManager &MAM)
{
    /* Phase 0: no injected wipes yet → nothing to verify.
     * Phase 2: will scan all memset calls tagged with mirage metadata;
     *          emit error and return PreservedAnalyses::none() if any
     *          lack isVolatile=true.
     */
    (void)M; (void)MAM;
    return PreservedAnalyses::all();
}

/* ── Pass registration ───────────────────────────────────────────────────── */
void mirage::registerAntiREAPasses(PassBuilder &PB)
{
    /* Load the profile from the environment variable set by antirea-opt.
     * This is the mechanism by which --profile JSON reaches the pass. */
    const char *profile_env = std::getenv("ANTIREA_PROFILE");
    MirageProfile profile = MirageProfile::loadOrDefault(profile_env);

    PB.registerOptimizerLastEPCallback(
        [profile](ModulePassManager &MPM, OptimizationLevel OL) {
            FunctionPassManager FPM;

            if (profile.pass1_ephemeral_str)
                FPM.addPass(EphemeralStrPass());

            if (profile.pass2_env_cff)
                FPM.addPass(EnvCFFPass());

            if (profile.pass3_poly_mba)
                FPM.addPass(PolyMBAPass());

            if (profile.pass5_dynamic_barrier)
                FPM.addPass(DynamicBarrierPass());

            MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

            if (profile.pass4_honeypot)
                MPM.addPass(HoneypotPass());

            /* Verifier always runs last — cannot be disabled.
             * Phase 2: MPM.addPass(VolatileWipeVerifier());
             * Phase 0: no injected wipes exist yet — verifier is a no-op. */
        }
    );
}

/* ── LLVM plugin entry point ─────────────────────────────────────────────── */
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
    return {
        LLVM_PLUGIN_API_VERSION,
        "AntiREA",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            mirage::registerAntiREAPasses(PB);
        }
    };
}
