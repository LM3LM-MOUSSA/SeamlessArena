# Research checklist before implementation

This document defines the research needed to begin implementing the Seamless Arena PvP extension. Each item has a concrete question and a deliverable. Mark an item complete only when the stated evidence is recorded in this repository.

**Project constraints:** C++ implementation; Visual Studio 2026 solution; native DLL loaded through Mod Engine 3 (`me3`); Elden Ring; built as an extension alongside Seamless Co-op; arena PvP inspired by Better Multiplayer / DEN Maps; investigate vanilla Colosseum arenas.

## 0. Freeze the compatibility target

- [ ] Record the exact Elden Ring executable version and Steam build ID to support first. **Deliverable:** `game-version.md` with `eldenring.exe` file version, build ID, game edition/DLC state, and date captured.
- [ ] Record the exact Seamless Co-op release/version and file hashes for `ersc.dll` and its settings/config files. **Deliverable:** compatibility table with release URL, version, hashes, and tested game build.
- [ ] Record the exact me3 release and profile format/version used to load the mod. **Deliverable:** me3 release URL, version, and a minimal profile snippet checked against the official profile docs.
- [ ] Decide and document the initial supported combinations (game × Seamless Co-op × me3). **Deliverable:** a compatibility matrix with unsupported combinations explicitly marked.

## 1. Audit the provided Better Multiplayer source

Use the local source the user identified at `C:\Users\Rayen_lm3alim\Desktop\ee\rs src MP ED\bettermultiplayer` as a design and implementation reference. Do not assume its code, game build, or license is compatible with this project.

- [ ] Read its `LICENSE`, README, build files, and dependency list before reusing any code. **Deliverable:** a source reuse decision: allowed, attribution/notice required, incompatible, or do not copy.
- [ ] Locate the entry point, DLL initialization/shutdown path, logging, configuration parsing, and game-version checks. **Deliverable:** source file + symbol/function references for each.
- [ ] Trace the complete match lifecycle in that source: create/join → ready → map transition → active match → death/respawn → result → cleanup. **Deliverable:** sequence diagram with source references.
- [ ] Find how it represents map selection, teams, player identity, spawn points, score, timer, and disconnects. **Deliverable:** data-flow table naming the concrete structs/functions and whether each is local, host-owned, or network-synchronized.
- [ ] Identify every game hook, pattern/signature, patched instruction, and direct memory access it uses. **Deliverable:** hook inventory with target function, purpose, version assumptions, and failure behavior.
- [ ] Identify what is specific to DEN Maps or its assets/params versus reusable PvP logic. **Deliverable:** separation list: game assets/data, map-specific logic, reusable match systems.
- [ ] Compare its lifecycle and hooks to Seamless Co-op and libER. **Deliverable:** reuse/adapt/reject table; no assumption that matching behavior means safe code reuse.

## 2. Establish Seamless Co-op integration boundaries

- [ ] Inspect the installed release documentation/config and identify whether it exposes a supported extension API, callbacks, or only a loadable DLL environment. **Deliverable:** documented API inventory with exact exported symbols/config entries; if none exists, record that extension points are not publicly documented.
- [ ] Determine the actual load order for `ersc.dll` and the arena DLL under the chosen me3 profile. **Deliverable:** profile snippet plus a startup log proving load order; choose explicit `load_before`/`load_after` relationships only if me3 supports and the experiment confirms them.
- [ ] Determine whether Seamless Co-op owns or changes player replication, network message routing, session ownership, world transitions, saves, death state, or multiplayer restrictions relevant to PvP. **Deliverable:** integration table with observed owner/modifier and evidence for each subsystem.
- [ ] Verify whether the arena mod can use existing Seamless sessions without changing the Seamless executable or DLL. **Deliverable:** a controlled compatibility result for one host and one joining client.
- [ ] Record failure behavior if Seamless is absent, disabled, initialized late, or at a different version. **Deliverable:** explicit startup policy: fail closed with a clear log, disable arena mode, or supported fallback.

## 3. me3 native DLL contract

- [ ] Read the official me3 native-mod/profile documentation and confirm the current schema for `[[natives]]`, `path`, `enabled`, `load_early`, `initializer`, `finalizer`, and ordering fields. **Deliverable:** minimal tested `.me3` profile entry for this DLL.
- [ ] Confirm the required DLL architecture (x64), exported initializer/finalizer names, calling convention, and when each is called relative to game startup/shutdown. **Deliverable:** ABI note and minimal native DLL that logs init/finalize once.
- [ ] Determine DLL search paths and runtime dependencies for release packaging. **Deliverable:** clean-machine dependency list, including MSVC runtime policy and whether any helper DLL must sit beside the mod.
- [ ] Determine how me3 reports load failures and where its logs/crash dumps are written. **Deliverable:** troubleshooting note with exact paths and a known-good / known-bad example.
- [ ] Check whether a finalizer is guaranteed on normal exit only, and design cleanup for crashes/forced termination. **Deliverable:** lifecycle table for normal exit, failed init, game update mismatch, and abrupt process exit.

## 4. libER and Elden Ring ABI / memory safety

- [ ] Read libER's version-specific symbol files, initialization path, supported game version list, and updater behavior. **Deliverable:** selected libER version/commit and a list of symbols/types required by the arena feature.
- [ ] Confirm C++ standard, compiler/toolset, x64 ABI, CRT/runtime library, iterator debug level, and static/dynamic linking requirements. **Deliverable:** written compatibility settings that match both this Visual Studio solution and the selected libER build.
- [ ] Trace libER initialization relative to me3 initializer timing and Elden Ring engine initialization. **Deliverable:** startup timing diagram and a minimal safe engine-query proof of concept.
- [ ] Identify engine calls that must run on the game thread versus worker/native task threads, especially world/map access, player state, and entity operations. **Deliverable:** thread-affinity table sourced from libER headers/docs and confirmed behavior.
- [ ] Identify lifetime and ownership rules for engine objects, references, allocators, and asynchronous tasks used by the mod. **Deliverable:** checklist of handles that may not be retained across map unload/shutdown.
- [ ] Decide exact behavior on a game version without matching libER symbols. **Deliverable:** version guard and user-facing log message; do not call unresolved engine functions.

## 5. Vanilla Colosseum map investigation

Map-name references currently list these IDs; the names are community reference data and must be verified against the target game build:

| Map ID | Listed name | Question to resolve |
| --- | --- | --- |
| `m45_00_00_00` | Royal Colosseum | Is this the playable arena MSB/map loaded for combat? |
| `m45_01_00_00` | Caelid Colosseum | Is this the playable arena MSB/map loaded for combat? |
| `m45_02_00_00` | Limgrave Colosseum | Is this the playable arena MSB/map loaded for combat? |
| `m45_00_00_99` | Royal Colosseum: Overworld | What does the `99` variant represent and when is it loaded? |
| `m45_01_00_99` | Caelid Colosseum: Overworld | What does the `99` variant represent and when is it loaded? |
| `m45_02_00_99` | Limgrave Colosseum: Overworld | What does the `99` variant represent and when is it loaded? |

- [ ] In Smithbox, verify for the target build whether each of the six IDs has a corresponding map/MSB and note the exact extracted file path/name. **Deliverable:** map inventory with game version, file names, and screenshots.
- [ ] For each `00`/`99` pair, compare MSB parts, models, regions, events, collision, navmesh, and entity IDs. **Deliverable:** per-pair diff table with counts and notable differences.
- [ ] Test whether `99` is actually an LOD representation. Require evidence such as lower-detail geometry or references from the main-world map; the `Overworld` label alone is insufficient. **Deliverable:** confirmed / rejected / unresolved conclusion with evidence.
- [ ] Record the map ID before entering a Colosseum, after entering, during a match, and after leaving. **Deliverable:** runtime transition log for Royal, Caelid, and Limgrave.
- [ ] Identify legal spawn positions, facing direction, collision safety, fall hazards, spectator positions, and map boundaries for each arena. **Deliverable:** spawn coordinate sheet and marked map screenshots.
- [ ] Identify whether vanilla arena rules, entrances, gates, matchmaking triggers, or event flags already set player restrictions or cleanup. **Deliverable:** event/region/flag inventory; state which vanilla behavior can be reused without modifying it.
- [ ] Establish whether map travel is synchronized by Seamless Co-op or by Elden Ring's native world transition. **Deliverable:** host/client transition experiment and logs.

## 6. PvP match rules as exact state transitions

Decide the first game mode before implementing generic mode abstractions. For the initial vertical slice, write a complete rules sheet for one mode (recommended: free-for-all deathmatch) with exact outcomes.

- [ ] Define participants, maximum player count, team assignment (or no teams), late join policy, and whether spectators count toward the cap. **Deliverable:** numeric rules table.
- [ ] Define match states and permitted transitions: `Idle → Queue/Join → Loading → Countdown → Active → SuddenDeath/Complete → Return/Cleanup`. **Deliverable:** state machine diagram including cancellation/error paths.
- [ ] Define ready checks, countdown length, match timer, score limit, tie handling, and simultaneous final-kill behavior. **Deliverable:** values or config defaults plus a deterministic tie-break rule.
- [ ] Define exact score events: player kill, assist (if included), suicide, environmental death, disconnect, draw, and invalid/duplicate death notification. **Deliverable:** event-to-score table with examples.
- [ ] Define which equipment/build restrictions are applied: consumables, Great Runes, spirit summons, buffs, healing, flask counts, gestures, Torrent, and loadout changes. **Deliverable:** each rule set to allowed/blocked/unchanged with implementation mechanism and evidence.
- [ ] Define respawn or elimination policy, invulnerability duration, spawn selection, spawn protection cancellation, and behavior when no safe spawn exists. **Deliverable:** precise timing and fallback rules.
- [ ] Define death, disconnect, host migration, player leaving arena, game pause, and returning to title behavior. **Deliverable:** transition table with match result and cleanup outcome for every case.
- [ ] Define victory announcement, scoreboard fields, result duration, and how all participants return to the originating world. **Deliverable:** UI/result/return sequence.

## 7. Multiplayer authority and synchronization

- [ ] Determine which player is authoritative for match creation, timer, score, phase, teams, and spawn selection. **Deliverable:** authority matrix (host/server versus each client) backed by observed Seamless behavior.
- [ ] Inventory any available Seamless message/packet API and its payload/versioning limits. **Deliverable:** message API note with exact callable functions/types; if unavailable, record the constraint and evaluate an independent protocol only after compatibility review.
- [ ] Specify the wire messages required for the first mode: match create/join, ready, phase change, score update, respawn request/result, disconnect, and match end. **Deliverable:** C++-independent protocol table listing fields, sender authority, validation, and version field.
- [ ] Decide how clients reject stale, duplicated, out-of-order, malformed, or unauthorized match events. **Deliverable:** validation rules and state sequence numbers.
- [ ] Measure network update cadence/latency under Seamless for player death and map transition; do not assume the local death callback arrives simultaneously on all clients. **Deliverable:** packet/log observations across host + at least two clients.
- [ ] Define reconciliation when client-local combat state disagrees with host match score. **Deliverable:** host-authoritative correction rule and visible error handling.
- [ ] Check whether a modded client without the arena DLL can join a Seamless session and what happens. **Deliverable:** compatibility result; reject or prevent incompatible match start explicitly.

## 8. Game events and gameplay hooks

- [ ] Identify the least invasive mechanism to observe player death, resurrection/respawn, map change, session join/leave, and game shutdown. **Deliverable:** hook/callback inventory with source, signature/symbol, calling thread, and game-version dependency.
- [ ] For each hook, record installation point, expected instruction bytes/signature, trampoline/relocation method, and cleanup/unhook behavior. **Deliverable:** hook safety sheet; no undocumented blind patching in the first milestone.
- [ ] Test that a missing/mismatched signature disables only the dependent feature and produces a useful log. **Deliverable:** failure test evidence.
- [ ] Determine whether death/respawn can be controlled through a libER API or requires observing vanilla behavior and changing game state. **Deliverable:** proof-of-concept decision with exact API/function references.
- [ ] Verify the order of vanilla death event, Seamless synchronization, match score update, and respawn. **Deliverable:** timestamped host/client event trace.

## 9. Configuration, logging, and recovery

- [ ] Specify the configuration file location, format, schema version, defaults, and invalid-value handling. **Deliverable:** `arena-config.example.toml` (or chosen format) and schema table.
- [ ] List only settings that need user control in the first mode: mode, arena, player/score cap, timer, respawn delay, rules toggles, and debug logging. **Deliverable:** option table with type, valid range, default, and effect.
- [ ] Define log categories and stable event IDs for initialization, version mismatch, map transition, match state changes, player events, network validation failures, and cleanup. **Deliverable:** logging table and sample session log.
- [ ] Define recovery after failed map load, failed player spawn, session desync, invalid config, or missing dependency. **Deliverable:** error-to-recovery table; never leave players stranded in an arena.
- [ ] Define save-file behavior and prove the arena does not write to the vanilla save or alter Seamless save extensions. **Deliverable:** before/after save-path and hash observation; no save edits in initial implementation.

## 10. Game data, asset ownership, and distribution

- [ ] Determine whether the feature needs only DLL logic or also regulation, event, map, text, or UI asset overrides. **Deliverable:** asset inventory with owner and load path.
- [ ] Identify conflicts with Seamless Co-op and other common ER mod packages for each required override. **Deliverable:** conflict table and an installation profile that preserves the user's existing packages.
- [ ] Check licenses and redistribution permissions for Better Multiplayer source, libER, Smithbox-derived data, and any bundled assets. **Deliverable:** `THIRD_PARTY_NOTICES.md` inputs and redistribution decision.
- [ ] Define package contents and installation/removal steps for me3, including how the user disables the arena native while keeping other mods. **Deliverable:** clean install/uninstall test checklist.

## 11. Build and validation gates

- [ ] Confirm the solution platform/toolset is x64 and Visual Studio 2026 can compile the selected C++ standard and libER commit. **Deliverable:** recorded IDE/toolset versions and successful Release build log.
- [ ] Verify `/MD` versus `/MT`, Debug versus Release ABI, iterator debug setting, PDB generation, and runtime dependency packaging. **Deliverable:** final build-settings table.
- [ ] Define the smallest in-game validation setup: offline launch through me3, one host + one client, supported Seamless build, one Colosseum, and a reversible mod profile. **Deliverable:** repeatable manual smoke procedure.
- [ ] Capture baseline behavior with only me3 + Seamless, then with the arena DLL loaded but inactive, then with one match. **Deliverable:** three startup/session logs for comparison.
- [ ] Validate host and client across map load, countdown, one kill, death/respawn, match end, return to world, and disconnect. **Deliverable:** signed-off scenario table; each row records expected and observed outcome.
- [ ] Repeat the compatibility smoke check after any Elden Ring, Seamless Co-op, libER, or me3 version change. **Deliverable:** release checklist requiring version matrix update.

## Start-work gate

Implementation of match behavior should begin only after these decisions are recorded:

1. Supported Elden Ring / Seamless Co-op / me3 versions.
2. Legal source-reuse decision for Better Multiplayer.
3. Confirmed me3 DLL initialization contract and load order.
4. Confirmed host/client authority and an available synchronization path.
5. Verified playable Colosseum map IDs and tested one arena transition on host and client.
6. One exact first-mode rules sheet, including death, score, respawn, disconnect, and cleanup.
7. libER/toolchain ABI compatibility and graceful version-mismatch behavior.

If an item is unknown, record the experiment needed and its result. Do not replace unknowns with assumptions in code.

## Primary references to inspect

- [libER source and README](https://github.com/Dasaav-dsv/libER) — C++ API, versioned symbols, build requirements, ABI notes, and examples.
- [libER generated documentation](https://dasaav-dsv.github.io/libER/) — API/type references.
- [Smithbox source](https://github.com/vawser/Smithbox) — map editor and game-data inspection implementation.
- [me3 official documentation](https://me3.readthedocs.io/) — install, profiles, native DLL configuration, launch behavior, and logs.
- [me3 source](https://github.com/garyttierney/me3) — profile/native loading implementation when documentation leaves a behavior unclear.
- [Seamless Co-op official release repository](https://github.com/LukeYui/EldenRingSeamlessCoopRelease) — release assets/version references; inspect the exact installed release and its documentation.
- [Seamless Co-op documentation](https://ersc-docs.github.io/) — documented user-facing behavior and configuration.
- [Souls Modding Wiki map-name list](https://www.soulsmodding.com/doku.php?id=er-refmat:map-name-list) — community map ID/name cross-reference; verify IDs against the target game files.
- [DSMapStudio map ID reference](https://dsmapstudio.com/editors/map-editor/map-ids) — map ID concepts and naming reference.
- [Project source: Better Multiplayer / DEN Maps](C:/Users/Rayen_lm3alim/Desktop/ee/rs%20src%20MP%20ED/bettermultiplayer) — local source provided by the user; inspect its license and exact source before reuse.

External mod/game references are research inputs, not proof that a specific behavior is supported by the project's target versions. Record the exact version and evidence for every conclusion.
