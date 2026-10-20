# AntiREA — Anti-Reverse-Engineering Compiler Pass Suite

**Internal alias:** `mirage` · **Library:** `libmirage_runtime`

AntiREA is a cost-asymmetry engine: an out-of-tree LLVM pass suite that
hardens native binaries (games, apps) against both classical reverse
engineering (IDA, Cheat Engine, GameGuardian) and MCP-driven autonomous
AI reverse-engineering agents (Ghidra + Frida/LLDB/QEMU/Unicorn via
MCP tool calls).

> **Not positioned as "unbreakable."** Every pass removes a cheap oracle,
> raises the compute cost of recovering ground truth, or seeds false
> confidence. Passes are independently toggleable and rotatable.

## Components

| Component | Role |
|---|---|
| `libAntiREAPass.so/.dll` | Out-of-tree LLVM plugin, runs at `OptimizerLastEPCallback` |
| `libmirage_runtime.a/.lib` | Static runtime: inline-syscall sensors, ChaCha20/XOR, FNV-1a |
| `antirea-opt` | Standalone IR optimizer CLI (`antirea-opt input.bc -o out.bc --profile=paranoid.json`) |
| `antirea-arena` | Python/MCP red-vs-blue genetic evaluation harness |

## Passes

| Pass | Flag | Effect |
|---|---|---|
| Pass 1 | `-antirea-ephemeral-str` | Ephemeral stack-scoped string hydration (fixes lazy-decrypt flaw) |
| Pass 2 | `-antirea-env-cff` | Branchless environment-entangled CFF (fixes crash-oracle flaw) |
| Pass 3 | `-antirea-poly-mba` | Polynomial mixed boolean-arithmetic obfuscation |
| Pass 4 | `-antirea-honeypot` | Effort-gated semantic honeypots + decompiler-directed strings |
| Pass 5 | `-antirea-dynamic-barrier` | Interrupt-on-dynamic-probe mid-function barrier |

## Targets

- **Linux x86_64** — inline syscalls, bypassing libc
- **Windows x86_64** — PEB walks + direct syscall stubs, bypassing ntdll exports

Both platforms built and tested in parallel within each phase.

## Building

### Prerequisites

- LLVM 19+ development headers (`llvm-dev` / LLVM installer)
- CMake 3.20+
- Ninja (recommended) or Make/MSBuild
- C++17 compiler (clang++ or MSVC 2022+)

### Linux

```bash
cmake -B build -G Ninja \
  -DLLVM_DIR=$(llvm-config --cmakedir) \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (Developer Command Prompt)

```cmd
cmake -B build -G Ninja ^
  -DLLVM_DIR="C:\Program Files\LLVM\lib\cmake\llvm" ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Using antirea-opt

```bash
antirea-opt input.bc -o protected.bc --profile=profiles/paranoid.json
clang protected.bc -o protected_binary
```

## Profiles

Located in `profiles/`:

| Profile | Use case |
|---|---|
| `paranoid.json` | Maximum protection, higher binary size |
| `balanced.json` | Default — Passes 1–3 stacked (per §5b empirical finding) |
| `light.json` | Minimal overhead, single-pass |

## License

MIT — see [LICENSE](LICENSE).
Third-party notices: [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES).
