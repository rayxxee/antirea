/**
 * AntiREAOptCLI.cpp — antirea-opt CLI implementation
 *
 * Orchestrates:
 *   1. Arg parsing (LLVM CommandLine + custom --profile / --dump-profile)
 *   2. Load input .bc → LLVMContext + Module
 *   3. Set ANTIREA_PROFILE env var so the pass plugin sees the profile
 *   4. Build PassManager with AntiREA passes registered
 *   5. Run pipeline
 *   6. Write output .bc
 */

#include "AntiREAOptCLI.h"
#include "../../lib/pass/AntiREAPass.h"
#include "../../lib/pass/MirageProfile.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <memory>
#include <string>

using namespace llvm;

/* ── CLI option declarations ─────────────────────────────────────────────── */
static cl::opt<std::string> InputFile(
    cl::Positional, cl::desc("<input.bc>"), cl::Required);

static cl::opt<std::string> OutputFile(
    "o", cl::desc("Output bitcode file"), cl::value_desc("output.bc"),
    cl::init("-"));

static cl::opt<std::string> ProfilePath(
    "profile", cl::desc("Path to protection profile JSON"),
    cl::value_desc("profile.json"), cl::init(""));

static cl::opt<bool> DumpProfile(
    "dump-profile", cl::desc("Print effective profile as JSON and exit"),
    cl::init(false));

static cl::opt<bool> VerifyOnly(
    "verify-only",
    cl::desc("Run volatile-wipe verifier only, no output (for CI checks)"),
    cl::init(false));

static cl::opt<OptimizationLevel> OptLevel(
    cl::desc("Optimization level:"),
    cl::values(
        clEnumValN(OptimizationLevel::O0, "O0", "No optimization"),
        clEnumValN(OptimizationLevel::O1, "O1", "Some optimization"),
        clEnumValN(OptimizationLevel::O2, "O2", "More optimization"),
        clEnumValN(OptimizationLevel::O3, "O3", "Maximum optimization")
    ),
    cl::init(OptimizationLevel::O2));

static cl::opt<bool> NoVerifyIR(
    "no-verify", cl::desc("Skip IR verification passes"), cl::init(false));

namespace mirage {

int antireaOptMain(int argc, char **argv)
{
    InitLLVM X(argc, argv);
    cl::ParseCommandLineOptions(argc, argv,
        "antirea-opt — AntiREA LLVM IR optimizer\n"
        "  antirea-opt input.bc -o output.bc --profile=paranoid.json\n"
    );

    /* ── Load and resolve profile ── */
    const char *profile_path = ProfilePath.empty() ? nullptr : ProfilePath.c_str();
    mirage::MirageProfile profile = mirage::MirageProfile::loadOrDefault(profile_path);

    if (DumpProfile) {
        llvm::outs() << profile.toJSON() << "\n";
        return 0;
    }

    /* Propagate profile path to pass plugin via environment */
    if (!ProfilePath.empty()) {
#if defined(_WIN32)
        _putenv_s("ANTIREA_PROFILE", ProfilePath.c_str());
#else
        setenv("ANTIREA_PROFILE", ProfilePath.c_str(), 1);
#endif
    }

    /* ── Read input bitcode ── */
    LLVMContext Ctx;
    auto BufOrErr = MemoryBuffer::getFile(InputFile);
    if (auto EC = BufOrErr.getError()) {
        WithColor::error() << "cannot open '" << InputFile << "': "
                           << EC.message() << "\n";
        return 1;
    }

    auto ModOrErr = parseBitcodeFile(BufOrErr->get()->getMemBufferRef(), Ctx);
    if (auto Err = ModOrErr.takeError()) {
        WithColor::error() << "cannot parse bitcode: ";
        logAllUnhandledErrors(std::move(Err), llvm::errs(), "");
        return 1;
    }
    std::unique_ptr<Module> M = std::move(ModOrErr.get());

    /* ── Verify input IR ── */
    if (!NoVerifyIR) {
        if (verifyModule(*M, &llvm::errs())) {
            WithColor::error() << "input IR verification failed\n";
            return 1;
        }
    }

    /* ── Build pass pipeline ── */
    PassBuilder PB;

    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    /* Register AntiREA passes at OptimizerLastEPCallback */
    mirage::registerAntiREAPasses(PB);

    ModulePassManager MPM;
    if (VerifyOnly) {
        /* Only run the verifier, not the full pipeline */
        MPM.addPass(mirage::VolatileWipeVerifier());
    } else {
        /* Build pipeline: LLVM standard optimization + AntiREA passes appended
         * via the OptimizerLastEPCallback registered above */
        MPM = PB.buildPerModuleDefaultPipeline(OptLevel);
    }

    MPM.run(*M, MAM);

    /* ── Verify output IR ── */
    if (!NoVerifyIR) {
        if (verifyModule(*M, &llvm::errs())) {
            WithColor::error() << "output IR verification failed (pass bug)\n";
            return 1;
        }
    }

    if (VerifyOnly) {
        llvm::outs() << "antirea-opt: verifier passed\n";
        return 0;
    }

    /* ── Write output bitcode ── */
    std::error_code EC;
    std::unique_ptr<ToolOutputFile> Out =
        std::make_unique<ToolOutputFile>(OutputFile, EC, sys::fs::OF_None);
    if (EC) {
        WithColor::error() << "cannot open output '" << OutputFile << "': "
                           << EC.message() << "\n";
        return 1;
    }

    WriteBitcodeToFile(*M, Out->os());
    Out->keep();

    llvm::outs() << "antirea-opt: wrote protected bitcode to " << OutputFile << "\n";
    return 0;
}

} /* namespace mirage */
