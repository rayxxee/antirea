# AntiREA — Implementation Specification
**Alias:** `mirage` (internal component prefix: `mirage-*`, library: `libmirage_runtime`)

**Status:** Draft v1 — engineering handoff spec
**Target audience:** autonomous build agent (e.g. Antigravity) implementing from this spec
**Scope:** Out-of-tree LLVM pass suite + runtime library + red/blue evaluation harness for hardening native binaries (games/apps) against both classical reverse engineering (IDA, Cheat Engine, GameGuardian) and MCP-driven autonomous AI reverse-engineering agents (e.g. tools combining Ghidra + Frida/LLDB/QEMU/Unicorn via MCP tool calls).

---

## 0. Design Philosophy

AntiREA is **not** positioned as "unbreakable." It is positioned as a cost-asymmetry engine: every pass should either (a) remove a cheap oracle the attacker currently relies on, (b) raise the compute/tool-call cost of recovering ground truth, or (c) seed false confidence that terminates the attacker's search early. Passes are designed to be **independently toggleable and independently rotatable** — see §6 (Robustness & Update Protocol) — because no single technique here is expected to remain effective indefinitely against an adversary that updates its own agent harness.

Threat model assumption: the attacker is an MCP-orchestrated LLM agent with tool access to a disassembler/decompiler, a dynamic instrumentation framework (Frida/LLDB), and possibly an emulator (QEMU/Unicorn), operating with a finite tool-call / reasoning budget per target function.

---

## 1. Problem Statement (baseline being defended against)

Prior art `qusamulab/kagura` targets human analysts and has three known gaps against autonomous agents:

1. **Lazy-decrypt string flaw** — strings decrypted once into static/global memory remain in plaintext in RAM for the rest of process life; a post-startup Frida attach reads them trivially.
2. **Branching anti-debug oracle** — `if (check_debug()) abort();` gives the agent a crash + stack trace, i.e. a free label for exactly which check to patch out.
3. **No defense against the LLM "plausibility & sunk-cost" bias** — agents stop at the first answer that looks right, especially one they had to "earn" via nontrivial tool-call effort (e.g. breaking a weak cipher).

Additionally, modern LLVM 19+ middle-end optimization (`SCCPPass`, `CorrelatedValuePropagationPass`, `SimplifyCFGPass`, `InstCombine`) will silently strip naive opaque predicates and constant-folded MBA unless those expressions are mathematically entangled with genuine runtime state the compiler cannot prove statically.

---

## 2. System Architecture

```
AntiREA/
├── libAntiREAPass.so        # out-of-tree LLVM pass plugin (clang/clang++/rustc)
├── libmirage_runtime.a       # static runtime: inline-syscall sensors, ChaCha20/XOR, FNV-1a
├── antirea-opt                # standalone IR optimizer CLI (kagura-opt equivalent)
└── antirea-arena               # Python/MCP red-vs-blue genetic evaluation harness
```

| Component | Hook point | Role |
|---|---|---|
| `libAntiREAPass.so` | `OptimizerLastEPCallback` | Runs after LLVM's own optimizer so protections aren't stripped, but before backend codegen so register allocation still blends them in naturally |
| `libmirage_runtime.a` | linked statically | Provides telemetry/crypto primitives without libc-hookable imports (no `ptrace`, `fopen`, `dlsym` calls visible in the IAT/GOT) |
| `antirea-opt` | CLI | `antirea-opt input.bc -o output.bc --profile=paranoid.json` — multi-language pipeline entry point |
| `antirea-arena` | offline | Genetic-algorithm-driven benchmarking: evolves per-function obfuscation parameters against a live agent (decompiler + Frida) in a closed loop |

---

## 3. Pass Specifications

### Pass 1 — `-antirea-ephemeral-str` (Ephemeral Stack-Scoped String Hydration)

**Fixes:** lazy-decrypt flaw (§1.1).

- Compile-time: every string literal `S` (length `L`) encrypted into `.rodata` with a per-string stream cipher (ChaCha20 or rolling split-key XOR), keyed by a random 64-bit seed `K_S` mixed with an expected-clean environment checksum `E_clean`.
  - `C[i] = S[i] XOR PRNG(K_S XOR E_clean, i)`
- At the use site: inject `alloca [L x i8]` on the local stack frame immediately before the consuming instruction, plus an inline decrypt loop.
- On every return / unwind edge of the function: inject `llvm.memset.p0.i64` with **`isVolatile = true`** to zero the buffer.
  - **Critical implementation note:** omitting `isVolatile` lets `DSEPass` prove the buffer is dead and deletes the wipe. This must be enforced as a pass invariant, not a convention — add a verifier sub-pass that fails the build if any injected wipe lacks the volatile flag.
- Result: plaintext string lifetime on the stack ≈ one function call duration, not process lifetime.

### Pass 2 — `-antirea-env-cff` (Branchless Environment-Entangled Control-Flow Flattening)

**Fixes:** crash-oracle flaw (§1.2).

Distributed micro-sensors (implemented in `libmirage_runtime`, each independently swappable):

| Sensor | Linux (x86_64) | Windows (x86_64) | Clean value |
|---|---|---|---|
| A — tracer presence | Inline `syscall` (bypassing libc) reading `TracerPid` from `/proc/self/status` | `NtQueryInformationProcess(ProcessDebugPort / ProcessDebugFlags / ProcessDebugObjectHandle)` called via direct syscall stub (not the `ntdll.dll` export, which is a hookable trampoline), plus a raw PEB read of `BeingDebugged` at `fs:[0x30]`/`gs:[0x60]` | `s1 = 0` |
| B — hook/instrumentation artifact scan | Scan thread names / `/proc/self/maps` for `frida`, `gum-js-loop`, `linjector`; check first bytes of hot libc functions for breakpoint/trampoline signatures (`0xCC`, `0xE9` rel-jmp) | Walk the PEB `Ldr` module list directly (not via `GetModuleHandle`, which can be hooked) for known instrumentation DLLs (`frida-gadget.dll`, hook-framework names); check for `0xCC`/`0xE9` prologue patches on hot ntdll/kernel32 functions | `s2 = 0x9E3779B9` |
| C — self-checksum | Rolling CRC32/FNV-1a over a 64-byte window of current function's machine code, computed relative to the function's own runtime base so ASLR/PIE relocation doesn't produce false positives | Same, relative to the module's runtime base from the PEB, not a hardcoded link-time address | matches build-time golden hash |
| D — timing-delta VM/emulation detection | Calibrate a baseline instruction-timing delta (`rdtsc` around a short sensitive-instruction sequence) against an expected bare-metal range; QEMU/Unicorn-style dynamic binary translation inflates this delta by roughly an order of magnitude (bare metal ≈150–300 cycles vs. emulated >1000 cycles is the commonly cited rule of thumb) | Same `rdtsc`-based approach; also cross-check against `QueryPerformanceCounter` delta since some Windows-targeting emulators/sandboxes patch `rdtsc` specifically but miss the QPC path | within calibrated bare-metal range |

**Platform note:** AntiREA targets Linux and Windows x86_64 only — no AArch64/Android sensor variants are in scope. Each sensor needs two concrete implementations (Linux syscall-based, Windows PEB/direct-syscall-based) behind a common `libmirage_runtime` interface; avoid routing through user-mode API wrappers that a hooking framework would intercept (e.g. prefer direct syscall stubs over `ntdll.dll` exports on Windows, same rationale as bypassing libc on Linux).

**On Sensor D:** this directly targets the "dynamic execution pivot" named in §1 — an agent spawning QEMU/Unicorn to brute-force-execute past static obfuscation is exactly the behavior this sensor is built to catch. It is also the easiest sensor for an attacker to patch around once discovered (timing checks are a well-known cat-and-mouse category — see §6), so treat its specific thresholds as rotation-schedule data, not a constant, and combine it with sensors A–C rather than relying on it alone.

**Note on Sensor C fragility:** must account for legitimate relocation/ASLR rebasing (hash the *relative* byte window post-relocation, not absolute addresses) or this becomes a false-positive generator on normal OS behavior, not just a detector of tampering.

For every basic-block transition `B_i → B_real`, synthesize a parallel transition to a decoy block `B_decoy`. No conditional branch is emitted — the next dispatcher state is computed algebraically:

```
Δ_tamper = (s1 != 0) OR (s2 != 0x9E3779B9)
state_next = ID(B_real) XOR ( sext(Δ_tamper) AND (ID(B_real) XOR ID(B_decoy)) )
```

`sext(Δ_tamper)` (LLVM `CreateSExt`, i1 → i32) turns the boolean into an all-zero or all-ones mask — zero `cmp`/`jne` instructions emitted. Clean → mask `0x00000000` → `state_next = ID(B_real)`. Tampered → mask `0xFFFFFFFF` → `state_next = ID(B_decoy)`.

**Design requirement beyond original spec:** `B_decoy` should not be a dead end. Route it to produce a value that is *functionally consumed* downstream (e.g. feeds a checksum that gates some benign-looking behavior) so an agent that validates its finding against a second oracle still gets a self-consistent (wrong) answer, not an obviously inert stub.

### Pass 3 — `-antirea-poly-mba` (Polynomial Mixed Boolean-Arithmetic)

**Fixes:** LLM context-window comprehension and SMT/program-synthesis simplification.

- Linear MBA basis over `Z_2^64` (standard identities — `x+y = (x|y)+(x&y)`, etc.), combined with:
- Invertible affine/polynomial permutation wrapping: for odd `a1` (so `gcd(a1, 2^64) = 1`), compute `a1^-1 mod 2^64` via extended Euclid, and transform any sub-expression `E(x,y)`:

```
Ẽ(x,y) = a1^-1 · (a1 · E_MBA(x,y) + a0) − a1^-1·a0   (mod 2^64)
```

- To defeat `InstCombine` constant-folding, multiply one zero-identity term by the runtime sensor `s1` (expected `0` at runtime), loaded through an opaque inline-asm barrier (`asm volatile("" : "+r"(s1))`) so the compiler cannot prove it's zero and cannot fold it away.

**Known limitation (flag this explicitly to whoever maintains this pass):** linear-MBA + affine wrapping is a known target of program-synthesis-based deobfuscators (SiMBA-class tools, oracle-guided synthesis). This pass raises cost; it does not provide asymptotic security. Plan for periodic rotation of the identity basis set and multiplier choices rather than treating this as "solved."

### Pass 4 — `-antirea-honeypot` (Effort-Gated Semantic Honeypots + Decompiler-Directed Strings)

**Fixes:** MCP-agent reconnaissance behavior (`list_functions`, `search_strings`, `decompile_function`), exploiting sunk-cost plausibility bias.

4a. **Effort-gated breadcrumbs:** clone the signature of a protected function, create a reachable decoy (the `B_decoy` target from Pass 2 plus a standalone high-signal-named symbol, e.g. `internal_verify_master_hmac`). Embed a fake secret behind a *deliberately solvable but nontrivial* cipher (unrolled 4-round Feistel, or RC4 with hardcoded seed) so the agent has to spend several tool calls to "earn" it — raising its confidence in the (wrong) result per the sunk-cost mechanism.

4b. **Decompiler-directed string injection, GA-evolved (short shelf-life module — see §6):** inject dead-code metadata strings formatted as decompiler diagnostic annotations intended to redirect an agent's tool-call budget. This is directly validated by published research: "Automatically Attacking Software Reverse Engineering AI Agents" (2026) demonstrates a genetic-algorithm-based prompt-generation technique (an AutoDAN-style adversarial search) that evolves injection strings placed as extraneous string-variable assignments in decompiled output, which reliably corrupts the analytical output of LLM-powered disassembly/decompilation agents (tested against GhidraMCP-style pipelines) without affecting the binary's real functionality. **Upgrade over hand-written strings:** rather than hardcoding injection text, `antirea-arena` (§4 Phase 6) should run the same GA-search loop internally and re-evolve the injection payload per build/profile against whatever agent stack it's benchmarked against — hand-written strings go stale the moment one is publicly disclosed, evolved ones can be regenerated on demand. Still treat this as a disposable, independently-togglable sub-module: it is the single most fragile component in this spec, since any operator who notices the pattern class will start sanitizing/escaping tool output before it reaches model context. Do not architecturally depend on it.

### Pass 5 — `-antirea-dynamic-barrier` (Interrupt-on-Dynamic-Probe)

**Fixes:** the general case of "agent executes code dynamically to verify a hypothesis," independent of what sensor flags it.

Commercial prior art (Jscrambler Code Integrity 8.6, 2026) describes "dynamic barriers" that specifically engage when an agent attempts to dynamically execute/step through code to test a static hypothesis, rather than only gating on a fixed check at function entry. Implement this as a thin coordination layer over Sensors A–D (§Pass 2) plus a step-counter/single-step-detection heuristic (abnormally fine-grained execution cadence, consistent with an attached debugger single-stepping rather than normal execution) that can trigger the same branchless decoy-routing used in Pass 2, mid-function rather than only at entry. This closes the gap where an agent passes the entry-point checks cleanly (because it attached *after* them) but then single-steps through the body.

---

## 4. Build Plan (phased, for the implementing agent)

**Phase 0 — Scaffolding**
- Fork/initialize repo structure above; set up out-of-tree LLVM pass build (CMake + LLVM 19+ dev headers), rename all `kagura`-derived identifiers to `antirea`/`mirage`.
- Stand up `antirea-opt` CLI skeleton (arg parsing, `.bc` in/out, `--profile=<json>` loading).

**Phase 1 — Runtime library**
- Implement `libmirage_runtime`: Linux inline-syscall primitives and Windows direct-syscall/PEB-walk primitives behind a common interface (§3, Pass 2 sensor table), ChaCha20/XOR stream cipher, FNV-1a/CRC32 rolling hash, sensor A/B/C implementations for both platforms.
- Unit tests for each sensor on each platform, independent of the compiler passes (sensors must be testable standalone).

**Phase 2 — Pass 1 (ephemeral strings)**
- Implement string literal collection + compile-time encryption.
- Implement stack hydration + volatile wipe injection.
- Build the verifier sub-pass that rejects any build missing a volatile wipe on an injected buffer.

**Phase 3 — Pass 2 (branchless CFF)**
- Implement dispatcher synthesis, decoy block generation, sext-mask state transition.
- Implement the "functionally consumed decoy" requirement (§3, Pass 2 design requirement).

**Phase 4 — Pass 3 (poly-MBA)**
- Implement identity basis library + affine wrapper + opaque barrier injection.
- Add a basis-rotation mechanism (config-driven, not hardcoded) from day one — this is required infrastructure for §6, not a nice-to-have.

**Phase 5 — Pass 4 (honeypots)**
- Implement decoy function cloning + solvable-cipher secret embedding.
- Implement decompiler-string injection as a separately togglable flag (`--profile` option `honeypot.decompiler_strings: bool`).

**Phase 5b — Pass 5 (dynamic barrier)**
- Implement step-cadence/single-step-detection heuristic and mid-function decoy routing.
- Implement Sensor D (timing-delta VM/emulation detection) in `libmirage_runtime`, with calibration run at first-launch to establish the bare-metal baseline range per target device class.

**Phase 6 — `antirea-arena` evaluation harness**
- Python/MCP harness: spins up target binary, attaches a live agent stack (headless Ghidra + Frida via MCP tool interface), scores time-to-recovery / false-positive rate / tool-call count / **cost-to-defeat (wall-clock time + token spend), per §5b**.
- Genetic algorithm layer: evolves per-function obfuscation parameters (which passes, which intensity, which sensors) against harness score, **and separately evolves Pass 4b's injection payloads per the AutoDAN-style GA-search described in §3**.

**Phase 7 — Integration + profiles**
- Ship `paranoid.json` / `balanced.json` / `light.json` profiles trading off binary size & runtime overhead vs. protection strength.
- CI: build-time verifier checks (volatile wipes, sensor coverage) block merges.

---

## 5. Testing & Evaluation Methodology

- **Static baseline:** does `objdump`/Ghidra auto-analysis recover meaningful CFG for protected functions? (target: no)
- **Dynamic baseline:** does a Frida script dumping memory post-startup recover any plaintext secret? (target: no, per Pass 1)
- **Crash-oracle test:** attach LLDB/Frida to a protected build; confirm zero process crashes/aborts attributable to tamper checks (target: zero — all routing goes through Pass 2's branchless dispatcher).
- **Agent-in-the-loop test (`antirea-arena`):** run the actual target agent stack against N protected functions; record (a) time/tool-calls to any claimed answer, (b) whether claimed answer is correct vs. honeypot, (c) false "success" rate.
- **Regression:** confirm `InstCombine`/`SCCPPass`/`SimplifyCFGPass` do not fold away Pass 2/3 output — add this as an automated LLVM-IR diff check in CI, not just a manual spot check.

---

## 5b. Evidence-Based Design Choices (from external research)

A few concrete, citable findings should directly shape implementation priorities:

- **Pass-stacking measurably works.** Promon's 2026 app threat report tested ten frontier models against Obfuscator-LLVM and found that stacking three passes (instruction substitution + control-flow flattening + bogus control flow) dropped average AI deobfuscation success to roughly the 8–26% range depending on input type, versus much higher success on single-pass or unobfuscated code — i.e. the compounding-cost philosophy in §0 is empirically supported, and AntiREA should default profiles to running Passes 1–3 together rather than offering them as true/false toggles people reach for individually.
- **AArch64/ARM currently has a natural AI-resistance edge**, per the same report, simply because the training-data corpus for reverse-engineering writeups and malware analysis skews heavily x86. **Not applicable here** — AntiREA targets Linux and Windows x86_64 only, so this advantage doesn't carry over; budget accordingly (don't assume the same obfuscation intensity buys equivalent AI-resistance on x86_64 that the Promon data measured on ARM). This is worth re-checking periodically: as more x86-specific agent training/fine-tuning happens, the gap Promon measured may narrow further in the other direction.
- **Heavy obfuscation reliably inflates agent compute/time cost even when it doesn't fully block the agent** (Elastic Security Labs, Quarkslab). This reframes "success" for AntiREA: the measurable goal in `antirea-arena` should include cost-to-defeat (tool calls, wall-clock time, token spend) as a first-class metric alongside binary pass/fail recovery, not just a binary "was the secret recovered" outcome.
- **Do not add a classic VM-based (bytecode virtualization) pass without a diversification plan.** Recent work (arXiv:2601.12916, 2026) shows that the structural components of VM-based obfuscation — dispatch routine, handler blocks, VM region — are now reliably identifiable via static analysis directly on LLVM IR, even before optimization. If VM-based obfuscation is added in a future phase, the dispatch/handler layout must be randomized per-build (not a fixed architectural pattern) specifically to avoid this IR-level fingerprint; otherwise it provides little benefit over Pass 2's branchless CFF for the added complexity cost.
- **Confirmed real-world framing, for citation accuracy:** Quarkslab's "Defeating AI-Assisted Reverse Engineering (or at Least Trying To)" (2026) is a genuine, verifiable blog post and its central finding is closer to "we never caught the agent defeating a protection" than "obfuscation is dead" — useful as an accurate pull-quote if this doc's framing is presented externally, in contrast to industry narratives claiming AI has made obfuscation pointless.

### References — Tier 1 (independently verified by fetching/searching the actual source)
- Quarkslab — "Defeating AI-Assisted Reverse Engineering (or at Least Trying To)" — blog.quarkslab.com — confirmed
- Promon — "App Threat Report 2026 Q1: The State of Code Obfuscation Against AI" — promon.io — confirmed
- Elastic Security Labs — "The Cost of Understanding: LLM-Driven Reverse Engineering vs Iterative LLM Obfuscation" — elastic.co/security-labs — confirmed
- Jscrambler — "Jscrambler Enhances Code Integrity Product to Be LLM-Resilient" (Code Integrity 8.6) — jscrambler.com/blog — confirmed
- "Automatically Attacking Software Reverse Engineering AI Agents" (GA/AutoDAN-style prompt injection against GhidraMCP-style agents, 2026) — arXiv:2605.30667 — confirmed
- "Digital Camouflage: The LLVM Challenge in LLM-Based Malware Detection" (Böke & Torka) — arXiv:2509.16671 — confirmed
- VM-obfuscation structural detectability via LLVM IR — arXiv:2601.12916 — confirmed
- **Rao, H., Dong, Z., Zhao, Y., Li, H., & Wang, H. — "Acoda: Adversarial Code Obfuscation for Defending against LLM-based Analysis" — arXiv:2606.11755 (June 2026), Huazhong University of Science and Technology / Beijing University of Posts and Telecommunications.** Confirmed real and confirmed on-topic: genetic-algorithm-driven adversarial obfuscation exploiting LLM safety-alignment and token-based processing, up to 70% attack success rate (induced refusal/misanalysis) across seven mainstream LLMs, negligible runtime overhead, semantics-preserving. Also presented at ICSE 2026 Industry Challenge Track. This directly supports Pass 4's honeypot/decoy design and is a stronger citation for Pass 4b than the GA-prompt-injection paper alone — worth citing both.
- **Yates, R. — "Obfuscation vs the Optimizer: An LLVM Middle-End Arms Race" — Quarkslab Technical Blog, 16 April 2026.** Confirmed real, confirmed author and date. Supports Pass 3's `OptimizerLastEPCallback` hook-point choice and the opaque-runtime-value requirement for defeating `InstCombine`/`SCCPPass`/`SimplifyCFGPass`/`GVNPass`.
- **Scrambler: Mixed Boolean Arithmetic Obfuscation Tool Using E-graph and Equality Expansion — arXiv:2603.03624 (March 2026).** Confirmed real, confirmed on-topic (e-graph-based MBA via "Equality Expansion," an extension of equality saturation, generating diverse semantically-equivalent expressions with equivalence guaranteed by construction). **Correction:** arXiv lists the authors as *Seoksu Lee and one other author* — not the three-author "Lee, S., An, S., & Cho, E.-S." attribution in the original reference list. Fix the author line before citing this externally. This is a legitimate, stronger alternative/complement to Pass 3's hand-rolled affine-polynomial wrapping — worth evaluating e-graph-based rule expansion as the Phase 4 rotation mechanism (§6) rather than only rotating fixed identity/multiplier tables.

### References — Tier 2 (established prior art, not re-fetched this pass; well-known in the literature)
- Junod, Rinaldini, Wehrli, Michielin — "Obfuscator-LLVM – Software Protection for the Masses" — SPRO 2015 (defines canonical CFF/BCF/substitution passes both Kagura and AntiREA build on)
- Collberg, Thomborson, Low — "Manufacturing Cheap, Resilient, and Stealthy Opaque Constructs" — POPL 1998 (Potency/Resilience/Stealth metrics — useful as a formal evaluation rubric for `antirea-arena`)
- Schloegel et al. — "Loki: Hardening Code Obfuscation Against Automated Attacks" — USENIX Security 2022
- Zhou, Main, Gu, Johnson — "Information Hiding in Software with Mixed Boolean-Arithmetic Transforms" — WISA 2007 (foundational MBA/invertible-polynomial-permutation theory)
- Tan et al. — "LLM4Decompile" — EMNLP 2024 / arXiv:2403.05286 (useful source for Re-executability/Edit-Similarity/CodeBLEU metrics `antirea-arena` can invert as scoring functions)
- Hu, Liang, Chen — "DeGPT: Optimizing Decompiler Output with LLMs" — NDSS 2024 (relevant to how LLMs use structural anchors/strings to infer semantics — the mechanism Pass 4 targets)

These six are standard, frequently-cited work in the obfuscation/decompilation literature and weren't independently re-fetched in this pass, but there's no particular reason to doubt them — unlike the Tier 1 list, which had fabrication risk specifically because the dates (2026) sit right at or past general knowledge limits.

## 6. Robustness & Update Protocol

This system is explicitly an arms race. Build these mechanisms in from the start rather than bolting them on later:

1. **Per-pass independence & toggles.** Every pass must be disablable/tunable via the `--profile` JSON without touching other passes. When one technique gets defeated or patched around, you rotate or disable it without a full rebuild of the suite.
2. **Rotation infrastructure for Pass 3 (MBA basis) and Pass 4a (cipher/seed choices).** These are the components most likely to degrade against improving synthesis/solver tooling — treat their parameter sets as data to be refreshed on a cadence (e.g. quarterly), not constants baked into the pass source.
3. **Explicit "shelf-life" tagging.** Mark Pass 4b (decompiler-string injection) and any other technique that relies on a specific, patchable agent-harness behavior as *tier-2, disposable* in the profile schema — distinct from *tier-1, structural* techniques (Pass 1, Pass 2) that remove real oracles rather than exploiting a specific implementation quirk.
4. **`antirea-arena` as a continuous signal, not a one-time test.** Re-run the red/blue harness against the current best-available agent stack on a regular cadence; treat a rising "agent success rate" trendline as the trigger for rotating Phase 4/5 parameters, independent of any single external disclosure.
5. **Citation hygiene.** Several design choices here are grounded in cited external research (Quarkslab, Acoda, Scrambler papers referenced in the originating proposal). Before publishing or relying on specific claims from them, verify the citations independently — do not propagate unverified claims into documentation that others will treat as authoritative.
6. **Don't over-index on any single pass.** No individual pass here is claimed to be unbreakable; the design goal is compounding cost across four independent, uncorrelated techniques so that defeating all of them simultaneously is substantially harder than defeating any one.

---

## 7. Open Questions for the Implementing Agent

- ~~Target platform priority~~ — **Resolved: Linux and Windows x86_64, no mobile/AArch64 in scope.** Build and test both platforms in parallel within each phase rather than treating one as primary and one as a later port, since the spec already carries dual implementations per sensor (§3, Pass 2).
- Rust (`rustc`) pipeline support — is Phase 0's "multi-language" claim in scope for v1, or Phase 2+?
- `antirea-arena`'s target agent stack — which specific MCP RE-agent tooling should the harness be built against for the first benchmark pass?
- License/distribution model (the originating proposal references building on an existing open-source project's architecture — confirm licensing compatibility before publishing).
