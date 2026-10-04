# ADR-003 — Two platforms: x64 and ARM64

Status: **accepted** · 2026-09-30

## Context

The owner's direction is that the solution builds for **x64 and ARM64**, and for nothing else.

The development machine is a Snapdragon X, an ARM64 CPU, where an x64 build runs only under emulation. Windows on ARM is also a real share of the PCs the game can run on. The dedicated server of ADR-002 runs in a Windows Server container. That is x64 in practice, but nothing in the server layer is tied to it.

R16 requires the instruction set to be stated in every project. `/arch:AVX2` is an x86 switch and means nothing on ARM64, so the setting is stated per platform.

## Decision

1. **The solution and every project have exactly two platforms, x64 and ARM64**, each with Debug and Release. There is no Win32/x86, no 32-bit ARM and no ARM64EC. ARM64EC exists to mix x64 and ARM64 code in one process, and nothing here needs that.
2. **The instruction set is stated per platform, in every project, identically in Debug and Release** (R16):
   - x64: `/arch:AVX2` (`AdvancedVectorExtensions2`).
   - ARM64: `/arch:armv8.0` (`CPUExtensionRequirementsARMv80`). This is the ARM64 baseline and also MSVC's default, but R16 asks for the setting to be written down, not inherited. Raising the floor to a later ARMv8.x is a new decision with its own cost to name, as AVX2's is in R16.
3. **Everything that is not the instruction set is the same on both platforms**: toolset, language standard, conformance, warnings, floating-point model, include paths and precompiled headers. §3's Debug/Release alignment rule applies in both directions: across configurations and across platforms.
4. **Code is portable between the two by default.** An intrinsic or other platform-specific code path sits behind `_M_X64` / `_M_ARM64` with an implementation for each platform, and never builds for only one of them.
5. **CI builds Debug|x64 only.** ARM64 is built by whoever changes something platform-specific, and by whoever ships. It is not a second CI build.

## Consequences

- Build output lands in `ARM64/` beside `x64/`. Both are ignored.
- The same float code may give different results on the two platforms wherever it reaches the standard library's math functions or DirectXMath, which are implemented separately for each. Neither platform contracts `a*b+c` into an FMA: `/fp:precise` generates no contractions, and no project passes `/fp:contract` (R16). ADR-002 already requires no cross-machine determinism, because only the server simulates. A replay from a seed and command log (ADR-002 §8) reproduces on the same build for the same platform, not across platforms.
- An ARM64-only defect cannot be caught by CI. The owner builds ARM64 natively on the development machine, which covers this in practice. If ARM64 starts breaking unnoticed, the fix is an ARM64 CI job, not a guard.
- A package the game links has to ship ARM64 binaries as well as x64 ones. Both of the game's linked packages do: MsQuic (ADR-004) and the PIX event runtime (ADR-005). In the Debug|x64 and Debug|ARM64 builds of 2026-09-30, `dumpbin /headers` reads `8664` and `AA64` for each DLL the builds copied. ADR-001's packaging tools run only at build time, and the game links nothing from them.

## What this forecloses

- A 32-bit build of any kind.
- x86-only intrinsics or inline assembly without an ARM64 counterpart.
- ARM64EC, unless a later ADR has a reason to load x64 code into an ARM64 process.
