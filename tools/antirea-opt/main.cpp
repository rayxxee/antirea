/**
 * antirea-opt — AntiREA standalone IR optimizer CLI
 *
 * Usage:
 *   antirea-opt input.bc -o output.bc [--profile=<json>]
 *                                     [--dump-profile]
 *                                     [--verify-only]
 *                                     [--passes=<pass-name,...>]
 *                                     [-O0|-O1|-O2|-O3]
 *
 * Equivalent to: clang -fpass-plugin=libAntiREAPass.so ... but as a
 * standalone tool that accepts LLVM bitcode directly. Useful for
 * multi-language pipelines (C, C++, Rust via --emit=llvm-bc).
 *
 * Phase 0: Arg parsing, .bc in/out, --profile loading, pass registration.
 *          Actual passes are stubs — output .bc == input .bc.
 */

#include "AntiREAOptCLI.h"

int main(int argc, char **argv)
{
    return mirage::antireaOptMain(argc, argv);
}
