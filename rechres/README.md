# SeamlessArena Research & Technical Specifications (`rechres`)

This directory contains research findings, architectural specifications, reverse-engineering analyses, and implementation blueprints for the **SeamlessArena** project.

Each document corresponds to a foundational research topic outlined in [docs/TODO.md](file:///f:/dev/SeamlessArena/docs/TODO.md) and [docs/RESEARCH_CHECKLIST.md](file:///f:/dev/SeamlessArena/docs/RESEARCH_CHECKLIST.md).

---

## 1. Topic Index & Audit Status

Per the project policy in [docs/TODO.md](file:///f:/dev/SeamlessArena/docs/TODO.md) (Phase 0), statuses strictly reflect demonstrated evidence. Claims are classified as:
- `verified in source` — Confirmed by active source code, headers, project files, or passing unit tests in the repository.
- `verified in game` — Observed executing in a running game session (requires recorded logs, screenshots, or memory dumps).
- `documented only` — Sourced from official or community documentation without independent local runtime verification.
- `hypothesis` — Architectural design assumption or educated deduction awaiting experimental proof.
- `unknown` — Explicitly unidentified technical detail (e.g. unknown byte signature or unexamined internal API).

| Topic ID | File | Subject | Audit & Verification Status |
|---|---|---|---|
| **00** | [00-compatibility-target.md](file:///f:/dev/SeamlessArena/rechres/00-compatibility-target.md) | Compatibility target freeze (Elden Ring, Seamless Co-op, ME3) | `documented only` (exact local build hashes `unknown`) |
| **01** | [01-better-multiplayer-audit.md](file:///f:/dev/SeamlessArena/rechres/01-better-multiplayer-audit.md) | Better Multiplayer & DEN Maps source audit & design learnings | `verified in source` (Rust reference layout) & `documented only` (licensing) |
| **02** | [02-seamless-coop-integration.md](file:///f:/dev/SeamlessArena/rechres/02-seamless-coop-integration.md) | Seamless Co-op runtime model, hooks, and boundary constraints | `documented only` (save isolation) & `unknown` (extension API) |
| **03** | [03-me3-native-contract.md](file:///f:/dev/SeamlessArena/rechres/03-me3-native-contract.md) | Mod Engine 3 native DLL loader contract & profile configuration | `verified in source` (C-ABI export signatures) & `documented only` (ME3 loader) |
| **04** | [04-liber-abi-memory-safety.md](file:///f:/dev/SeamlessArena/rechres/04-liber-abi-memory-safety.md) | libER integration, FromSoftware engine ABI, and memory safety | `verified in source` (libER compiled & linked, EngineBridge, _ITERATOR_DEBUG_LEVEL=0 ABI enforced) |
| **05** | [05-colosseum-map-investigation.md](file:///f:/dev/SeamlessArena/rechres/05-colosseum-map-investigation.md) | Vanilla Colosseum maps, scene transitions, and map ID analysis | `documented only` (map names) & `hypothesis` (`_99` role, integer encoding) |
| **06** | [06-pvp-match-rules-state-machine.md](file:///f:/dev/SeamlessArena/rechres/06-pvp-match-rules-state-machine.md) | Deterministic match rules, state machine, and mode specifications | `verified in source` (Core engine & tests) & `documented only` (vanilla rules) |
| **07** | [07-multiplayer-authority-sync.md](file:///f:/dev/SeamlessArena/rechres/07-multiplayer-authority-sync.md) | Host-authoritative synchronization, wire protocol & network model | `verified in source` (packet schema) & `hypothesis` (host sync model) |
| **08** | [08-game-events-hooks.md](file:///f:/dev/SeamlessArena/rechres/08-game-events-hooks.md) | Game event observation, death detection, and memory hooking | `verified in source` (MinHook backend, native CSEzTask game loop tick) & `hypothesis` (ChrDeadEvent signature) |
| **09** | [09-configuration-logging-recovery.md](file:///f:/dev/SeamlessArena/rechres/09-configuration-logging-recovery.md) | Configuration schema, diagnostics logging, and emergency recovery | `verified in source` (TOML config & logger) & `hypothesis` (anti-stranding warp) |
| **10** | [10-assets-licensing-distribution.md](file:///f:/dev/SeamlessArena/rechres/10-assets-licensing-distribution.md) | Asset requirements, IP licensing boundaries, and distribution | `verified in source` (clean-room layout) & `documented only` (vanilla asset bounds) |
| **11** | [11-build-and-validation-gates.md](file:///f:/dev/SeamlessArena/rechres/11-build-and-validation-gates.md) | Visual Studio 2026 build pipeline and validation smoke tests | `verified in source` (CI tests passing) & `documented only` (in-game gates) |

---

## 2. Granular Claim Verification Table

This table audits concrete technical claims across the codebase and documentation to track evidence gaps:

| Claim Category | Specific Claim Statement | Document Reference | Evidence Status | Evidence Source / Current State |
|---|---|---|---|---|
| **ME3 Loader** | Profile uses `[[natives]]` with `path`, `load_early`, `initializer`, and `finalizer`. | [03-me3-native-contract.md](file:///f:/dev/SeamlessArena/rechres/03-me3-native-contract.md) | `documented only` | Gary Tierney's ME3 configuration reference and issue discussions. |
| **ME3 Loader** | `SeamlessArena_Initialize` exported with C ABI is invoked by ME3 without crashing. | [03-me3-native-contract.md](file:///f:/dev/SeamlessArena/rechres/03-me3-native-contract.md) | `verified in source` | Implemented in [me3_entry.cpp](file:///f:/dev/SeamlessArena/src/SeamlessArena.Native/me3_entry.cpp), compiled and linked with `/EXPORT` symbols in `SeamlessArena.dll`. |
| **ME3 Loader** | Runtime load order: `ersc.dll` loads before `SeamlessArena.dll` in a live process. | [02-seamless-coop-integration.md](file:///f:/dev/SeamlessArena/rechres/02-seamless-coop-integration.md) | `hypothesis` | Stated in ME3 docs; pending verification via live ME3 launch log. |
| **Seamless API** | `ersc.dll` exposes a supported custom packet broadcast/callback API. | [02-seamless-coop-integration.md](file:///f:/dev/SeamlessArena/rechres/02-seamless-coop-integration.md), [07](file:///f:/dev/SeamlessArena/rechres/07-multiplayer-authority-sync.md) | `unknown` | Seamless Co-op headers/docs do not publish an official plugin API. |
| **Seamless API** | `ersc.dll` uses Steam Networking Sockets and isolates `.co2` saves from vanilla `.sl2`. | [02-seamless-coop-integration.md](file:///f:/dev/SeamlessArena/rechres/02-seamless-coop-integration.md) | `documented only` | Yui Amoroll's Seamless Co-op documentation and release notes. |
| **Game Timing** | Game state mutations must execute on the main logic thread, not worker threads. | [04-liber-abi-memory-safety.md](file:///f:/dev/SeamlessArena/rechres/04-liber-abi-memory-safety.md) | `documented only` | FromSoftware engine architecture (FD4/Dantelion Havok task scheduler analysis). |
| **Map IDs** | `m45_00_00_00` (Leyndell), `m45_01_00_00` (Caelid), `m45_02_00_00` (Limgrave) are Colosseums. | [05-colosseum-map-investigation.md](file:///f:/dev/SeamlessArena/rechres/05-colosseum-map-investigation.md) | `documented only` | Souls Modding Wiki map list. |
| **Map IDs** | `m45_*_00_99` are LOD maps rather than separate scenes or overworld representations. | [05-colosseum-map-investigation.md](file:///f:/dev/SeamlessArena/rechres/05-colosseum-map-investigation.md) | `hypothesis` | Unverified. [docs/colosseum-map-id-research.md](file:///f:/dev/SeamlessArena/docs/colosseum-map-id-research.md) cautions that the label is "Overworld", not "LOD". |
| **Map IDs** | Runtime memory map ID integers are encoded as `0x45000000`, `0x45010000`, `0x45020000`. | [05-colosseum-map-investigation.md](file:///f:/dev/SeamlessArena/rechres/05-colosseum-map-investigation.md) | `hypothesis` | Used in [ArenaCoordinator.cpp](file:///f:/dev/SeamlessArena/src/SeamlessArena.Native/ArenaCoordinator.cpp#L425) as a placeholder. Runtime integer representation is unverified. |
| **Entity IDs** | `PlayerIns` pointers can be mapped to Steam IDs across loading transitions. | [04-liber-abi-memory-safety.md](file:///f:/dev/SeamlessArena/rechres/04-liber-abi-memory-safety.md) | `hypothesis` | Established pattern in modding; specific struct offsets require libER validation. |
| **Hooks** | `ChrDeadEvent` can be intercepted via MinHook to detect deaths before reload. | [08-game-events-hooks.md](file:///f:/dev/SeamlessArena/rechres/08-game-events-hooks.md) | `hypothesis` | Conceptually sound; exact function address and assembly signature are `unknown`. |
| **Hooks** | Vanilla death penalty (rune loss, loading screen) can be suppressed safely in a Colosseum. | [08-game-events-hooks.md](file:///f:/dev/SeamlessArena/rechres/08-game-events-hooks.md) | `hypothesis` | Requires in-game experimentation before enabling suppression. |
| **Network Authority** | The Seamless party host can act as authoritative arbiter of match rules and timers. | [07-multiplayer-authority-sync.md](file:///f:/dev/SeamlessArena/rechres/07-multiplayer-authority-sync.md) | `hypothesis` | Follows Seamless host-authoritative P2P session model; packet exchange untested. |
| **Match Rules** | Deterministic Duel, Combat Ordeal, and United Combat rules execute correctly. | [06-pvp-match-rules-state-machine.md](file:///f:/dev/SeamlessArena/rechres/06-pvp-match-rules-state-machine.md) | `verified in source` | Verified by automated tests in [SeamlessArena.Tests](file:///f:/dev/SeamlessArena/src/SeamlessArena.Tests/main.cpp) (6/6 passing). |

---

## 3. Executive Summary of Architectural Principles

1. **Architecture Decoupling:** `SeamlessArena.Core` is a pure C++20 library with zero game, DirectX, or hook dependencies. All game-specific logic is isolated in `SeamlessArena.Native`.
2. **Vanilla Colosseum First:** No bundled map assets or modified regulation bins. Relies strictly on vanilla Colosseum scenes (`m45_00_00_00`, `m45_01_00_00`, `m45_02_00_00`).
3. **Fail-Closed Safety:** If game executable versions, Seamless versions, or symbol offsets mismatch, the mod aborts initialization gracefully without installing hooks.
4. **Host Authority:** The Seamless party host arbitrates match states, timers, and scoring.
