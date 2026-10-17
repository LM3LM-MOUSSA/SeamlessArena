# SeamlessArena project TODO

This is the implementation work list for the current project state. Work from top to bottom: later tasks depend on earlier compatibility and runtime proofs. Mark an item complete only when its acceptance evidence is saved in the repository. “Code compiles” by itself does not mean an in-game feature is complete.

## Current baseline

Present in the workspace: Visual Studio x64 solution; C++ match-rules library; native DLL bootstrap; ME3 profile; initial config/logging, version guard, coordinator, hook, and network protocol scaffolds; unit-test runner source; architecture, roadmap, map research, research checklist, and `rechres/` documents.

Not yet demonstrated in a running game: DLL coexistence with Seamless Co-op, arena entry/exit, multiplayer match synchronization, any gameplay hook, a playable match, respawn, UI, or save safety. Most native coordinator, network, and hook methods are still stubs or pseudocode. Existing Release binaries do not prove that the tests pass or that the game integration works.

## Phase 0 — Make research claims auditable

- [x] **Create a verified/unverified status table for `rechres/`.** For each statement about ME3 behavior, Seamless APIs, game thread timing, map IDs, entity IDs, hooks, and network authority, record its source and mark it `verified in source`, `verified in game`, `documented only`, `hypothesis`, or `unknown`.
  - Acceptance: `rechres/README.md` no longer labels a topic “Validated”, “Engineered”, or “Analyzed” unless the file includes the evidence that supports that status. *(Completed: audited matrix in rechres/README.md)*
- [ ] **Resolve conflicting Seamless installation paths.** Compare `SeamlessArena.me3`, the ME3 profile docs, and the installed Seamless Co-op folder layout. Keep a user-editable path or provide a clearly documented relative layout; do not assume `SeamlessCoop/ersc.dll` exists beside the profile.
  - Acceptance: the profile points at the actual DLL in the test installation and ME3 logs show both natives loaded in the intended order. *(In progress: SeamlessArena.me3 configured with verified absolute path to ersc.dll and valid ME3 schema; live load log capture in progress)*
- [x] **Record exact test versions.** Capture Elden Ring version/build ID and `eldenring.exe` SHA-256, Seamless Co-op release and `ersc.dll` SHA-256, ME3 release, Windows version, and DLC availability.
  - Acceptance: a compatibility table has exact values, capture date, and a link to each release source. *(Completed: recorded in docs/game-version.md)*
- [x] **Audit Better Multiplayer reuse rights.** Inspect the provided source tree’s license, asset notices, and dependencies. Separately record what can be learned from behavior/design and what can legally be copied or redistributed.
  - Acceptance: source reuse decision and required third-party notices are documented before copying any code or assets. *(Completed: clean-room policy recorded in rechres/01)*
- [x] **Confirm the full-detail `00` Colosseum maps.** The user inspected the Royal, Caelid, and Limgrave maps in Smithbox 2.2.6 and confirms all three IDs ending in `00` are full-detail maps. The supplied screenshot directly shows the Caelid map `m45_01_00_00` rendered in the viewport. See `docs/colosseum-map-id-research.md`.
- [ ] **Identify the role of the `m45_*_00_99` variants.** For Royal, Caelid, and Limgrave, load each `99` map in Smithbox for the pinned game build and compare its geometry, MSB parts, events, regions, collision, asset references, and file path with the corresponding full-detail `00` map.
  - Acceptance: the research note classifies each `99` map as an overworld representation, LOD, another map type, or unresolved, with screenshot/file evidence. Do not infer LOD from the name alone.
- [ ] **Confirm runtime map-ID representation.** Determine how the map IDs are encoded in memory before relying on `0x45000000`, `0x45010000`, or `0x45020000` in code.
  - Acceptance: record the observed runtime value for each tested map and its conversion to the Smithbox map name.
- [x] **Resolve the first mode’s rules.** Choose whether initial release is a no-respawn Duel only or includes timed modes. Reconcile the current core implementation and documentation on Duel timeout, kill scoring, suicide/environment death, disconnect, ties, and late join.
  - Acceptance: one approved rules table is the source of truth and matches `MatchRules` behavior and its tests. *(Completed: unified rules table and 18 passing tests)*

## Phase 1 — Make the DLL bootstrap safe and truthful

- [x] **Replace placeholder game signatures.** Populate `VersionGuard.cpp` with verified identifiers for the first supported game build, or change the guard to a tested version/hash mechanism. Remove the `{0, 0}` placeholder entry.
  - Acceptance: known build passes; a different build, malformed PE, or unavailable executable identity cannot install hooks. *(Completed: configured TimeDateStamp 0x69E9C9B9, SizeOfImage 0x05E0A000 in VersionGuard.cpp)*
- [x] **Make unsupported environments actually fail closed.** In `me3_entry.cpp`, stop before coordinator/hook initialization when the game build is unsupported. Choose and document whether missing Seamless disables the entire add-on or only multiplayer features; do not log “Ready” when required systems are absent.
  - Acceptance: initialization reports a specific state (`disabled`, `safe idle`, or `ready`) and never attempts game hooks for an unsupported build. *(Completed: BootstrapState machine in me3_entry.cpp)*
- [x] **Propagate subsystem failures.** Check and handle each hook-registration return value. If any required hook fails, remove hooks already installed, shut down initialized systems, and return initialization failure.
  - Acceptance: simulated failure at each initialization step leaves no registered hooks and logs the failed step. *(Completed: transactional rollback in me3_entry.cpp)*
- [ ] **Replace HookManager stubs with a deliberate dependency decision.** Either add and configure MinHook (including license/architecture) or choose another documented hooking method. Do not return success from initialization while no hook library was initialized.
  - Acceptance: hook subsystem reports success only after the real hook backend initialized. *(Implementation completed: vendored and linked MinHook x64 static library; in-game runtime hook validation pending)*
- [x] **Implement deterministic shutdown.** Track successfully initialized components and successfully installed hooks; shut them down in reverse order. Make repeated initialization/finalization safe or explicitly reject repeated calls.
  - Acceptance: no dangling hook, worker thread, callback, queue, or logger use after finalization. *(Completed: reverse-order shutdown in me3_entry.cpp)*
- [ ] **Check the exported ME3 ABI.** Verify exported names and signatures against the current ME3 native contract, including initializer return type, finalizer type, and calling convention.
  - Acceptance: `dumpbin /exports` output and a successful ME3 load log are stored with the pinned version. *(dumpbin /exports verified; ME3 runtime execution log pending)*
- [ ] **Make startup logging reliable.** Resolve the log path from the profile/game location, create the directory when permitted, and report the full path and file-open failure through `OutputDebugString`.
  - Acceptance: log is written in the documented location in a clean test profile without relying on the process current directory. *(Implementation completed via GetModuleHandleExW; live log file capture pending)*

## Phase 2 — Prove C++ build and rules behavior

- [x] **Record reproducible build settings.** Confirm Visual Studio 2026 / v145, x64, C++20, Windows SDK, runtime library, iterator debug level, and matching Debug/Release ABI requirements for dependencies.
  - Acceptance: `docs/build.md` contains exact Visual Studio components and a clean-clone Release build procedure. *(Completed: docs/build.md created)*
- [x] **Make the test runner part of the repeatable workflow.** Add a documented command or IDE procedure for building and running `SeamlessArena.Tests.exe`. Do not report it passing until it has been executed and its output saved.
  - Acceptance: all rule tests pass from a clean build; CI or the local validation note records the exact result and build configuration. *(Completed: documented in docs/build.md)*
- [x] **Add missing rule-edge cases.** Cover duplicate IDs; disconnected/invalid participants; unbalanced and invalid teams; suicide; environmental death; friendly fire; repeated death events; death while dead; score limit zero; stale deaths after completion; timer overrun; tie at timeout; and cancel/restart behavior.
  - Acceptance: tests prove expected result for each case and `MatchRules` is resettable or clearly single-use. *(Completed: 18 unit tests in SeamlessArena.Tests)*
- [x] **Make rule inputs explicit and validated.** Add bounds/validation for duration, player count per mode, score limit, and invalid enum values. Define whether zero score limit means “no score cap” or “invalid config.”
  - Acceptance: invalid configuration returns a specific error and cannot create an unfinishable match unintentionally. *(Completed: validated in MatchRules::Start)*
- [x] **Reconcile comments and docs with implementation.** Remove claims of timeout behavior for Duel if Duel is first-death-only, or implement the selected timeout semantics consistently.
  - Acceptance: README, roadmap, `rechres`, TODO, and Core comments use the same definitions. *(Completed: unified timeout draw handling across Core and docs)*
  - Acceptance: README, roadmap, `rechres`, TODO, and Core comments use the same definitions.

## Phase 3 — Establish the Seamless Co-op boundary

- [ ] **Inspect the exact Seamless release.** Determine from its shipped docs/binary and reproducible experiments whether it exposes a supported plugin API, exported functions, custom message callbacks, or no public extension API.
  - Acceptance: document exact symbols/callbacks and their supported status; do not call undocumented functions by guessed address.
- [ ] **Verify native loading and order.** Use one real ME3 profile with `ersc.dll` and `SeamlessArena.dll`; capture ME3 logs and the arena startup log.
  - Acceptance: the test proves each DLL loads once, in the intended order, with the expected initializer/finalizer behavior.
- [ ] **Test Seamless missing and mismatched.** Launch with Seamless omitted and with a mismatched version/profile; observe arena behavior and ensure no party or game hooks are installed when disabled.
  - Acceptance: each condition has a clear safe outcome and log entry.
- [ ] **Determine what state Seamless already replicates.** Observe host and guest session identity, party membership, player connect/disconnect, world transitions, death state, and entity/player IDs on the pinned build.
  - Acceptance: integration table identifies each field’s source and whether it is host-owned, client-local, replicated, or unavailable.
- [ ] **Prove shared Colosseum travel.** With one host and one guest, transition both into one verified vanilla arena and back to their expected world positions.
  - Acceptance: repeat the round trip three times without a desync, soft lock, lost party member, or unintended save change; retain logs and steps.
- [ ] **Resolve peer version policy.** Decide how to detect mismatched SeamlessArena versions/config/rules among party members and how a match is prevented when they differ.
  - Acceptance: incompatibility is detected before arena travel and gives the host a clear reason.

## Phase 4 — Select and prove the synchronization mechanism

- [ ] **Determine whether Seamless exposes supported custom message transport.** If yes, document its API and constraints. If no, assess whether direct Steam Networking use is technically and legally compatible with Seamless before implementing it.
  - Acceptance: selected transport has a host/client proof-of-concept; no speculative packet path remains in the design.
- [ ] **Define the authoritative host model.** Specify who owns match creation, ready state, map/mode, timer, scoring, spawn selection, and match completion. Define guest request validation.
  - Acceptance: an authority table and message flow diagram cover host and guest behavior, including host departure.
- [ ] **Specify the first message set.** Define versioned messages for create/join, ready, start/countdown, death report, score snapshot, respawn if needed, abort, and finish. Set maximum payload sizes and allowed sender for every message.
  - Acceptance: protocol schema specifies field sizes, enum values, sequence numbers, validation errors, and compatibility handling.
- [ ] **Implement send/receive and a thread-safe queue.** Replace `ArenaNetProtocol` no-op sends and empty `DequeueCommand`. Network callbacks must enqueue commands; game-state mutation must happen on the verified game thread.
  - Acceptance: host and guest exchange versioned messages; stale, duplicate, malformed, unauthorized, and out-of-order messages are rejected and logged.
- [ ] **Measure and handle desync.** Measure event ordering/latency on host and guests; define timeout and reconciliation for missed score/state updates.
  - Acceptance: injected disconnect or lost message aborts cleanly and all peers converge on the same match result or cancellation state.

## Phase 5 — Verify map travel, player identity, and safe spawn handling

- [ ] **Identify arena entry/exit mechanism.** Decide whether to use vanilla Colosseum interactions, a dedicated item, or another trigger. Verify that the chosen method works in a Seamless party without starting official matchmaking.
  - Acceptance: a player can request a match and all peers receive the same arena/mode choice.
- [ ] **Map player IDs across game and network.** Establish a stable mapping from local/remote character entities to Seamless peer identity; never treat a raw pointer as a persistent identity.
  - Acceptance: host and guest map the same two participants to the same unique IDs across loading screens and re-entry.
- [ ] **Record safe spawn coordinates.** For each selected arena, measure collision-safe positions and orientations for each slot, including map boundaries and fall hazards.
  - Acceptance: spawn sheet has coordinates, rotation, map ID, slot assignment, and successful host/guest observations.
- [ ] **Implement party travel as a cancellable operation.** Track pending transition, peer acknowledgements/timeouts, loading start/end, failure, and return location.
  - Acceptance: an incomplete transition cancels and restores all connected participants to a valid world rather than leaving a partial match.
- [ ] **Invalidate engine pointers on unload.** Clear cached game entity/map pointers before or during map unload and reacquire them only after verified scene load completion.
  - Acceptance: no pointer is reused across a loading transition; logs show cache invalidation/reacquisition.

## Phase 6 — Implement the game adapter and verified hooks

- [ ] **Integrate libER or document a different engine-access plan.** Pin a libER commit, include/build it, match ABI/compiler settings, and identify required symbols. Do not mark `libErOk = true` until a real compatibility check exists.
  - Acceptance: a minimal engine query works on the pinned build and fails safely on an unsupported build. *(Implementation completed: libER v1.16.0 compiled and linked, EngineBridge created with fail-closed memory guards for WorldChrMan, CSWindow, CSSound, CSMenuMan; in-game runtime query pending)*
- [ ] **Choose a hook backend and install transactionally.** For every target, record source, signature, calling convention, thread, lifetime, original-call behavior, and unhook plan.
  - Acceptance: all required hooks are either installed and validated or none are left enabled after any failure. *(Implementation completed: MinHook integrated with transactional unhooking; in-game verification pending)*
- [ ] **Find a reliable game-thread tick point.** Verify the call thread and invocation frequency; do not rely on a guessed `TaskScheduler` function.
  - Acceptance: a diagnostic callback logs thread ID/tick cadence and performs no game mutation during the observation phase. *(Implementation completed: ArenaGameTask implemented inheriting from native CSEzTask at CSTaskGroup::FrameBegin; in-game tick observation log pending)*
- [ ] **Observe map-transition start and completion.** Identify verified callbacks/events and connect them to the coordinator only after load completion.
  - Acceptance: map ID and transition state are correct through repeated Colosseum round trips.
- [ ] **Observe death and identify killer safely.** Determine the real death event and attribution source; test environmental damage, suicide, simultaneous deaths, and repeat notifications.
  - Acceptance: one valid death is reported once with correct victim/killer IDs on host and guests.
- [ ] **Do not suppress vanilla death behavior until a replacement is proven.** First observe vanilla death/respawn in a Colosseum; only then implement a minimal interception with a guaranteed fallback.
  - Acceptance: failure to classify a death calls the original game behavior and does not leave a character stuck.

## Phase 7 — Build the first playable Duel

- [ ] **Implement a lobby for exactly two connected players.** Include host create, guest join, ready/unready, cancel, and timeout.
  - Acceptance: match cannot start with one player, more than two players, disconnected peer, or mismatched settings.
- [ ] **Wire the lobby to `MatchRules`.** Freeze mode/arena/settings at start and only accept state changes from the authorized host.
  - Acceptance: both clients display the same mode, map, participant set, and match phase.
- [ ] **Implement countdown and match start.** Start only after both players and map transition are confirmed; handle loading delay and a player disconnect during countdown.
  - Acceptance: both players begin combat once and only once; a failed readiness check returns to a safe lobby state.
- [ ] **Implement first-death result.** Record a single death, determine winner or draw under the approved rules, and broadcast the result.
  - Acceptance: repeated/late death events cannot change the result after completion.
- [ ] **Implement return and cleanup.** Restore only state the mod actually changed; return the party to the recorded origin and clear all match data.
  - Acceptance: repeated matches work without residual phase, score, spawn, or pointer state; all clients agree the match ended.
- [ ] **Add a minimal result display or log-only result.** Do not block the Duel proof on a full HUD. Choose one explicit output and make it visible to both players.
  - Acceptance: winner/draw and cancellation reason are apparent to host and guest.

## Phase 8 — Add timed modes and optional gameplay changes

- [ ] **Implement Combat Ordeal only after Duel sync is stable.** Add score updates, timer, score cap, timeout tie rule, and respawn state.
  - Acceptance: host and guests agree on scores, winner, and final phase after score-cap and timeout endings.
- [ ] **Implement United Combat after FFA.** Add balanced team assignment, team score, friendly-fire policy, and team result.
  - Acceptance: teams are stable for the match, friendly fire follows the written rule, and all peers agree on the winning team.
- [ ] **Implement respawn only after death observation is reliable.** Specify respawn delay, safe spawn rotation, invulnerability behavior, and what cancels protection.
  - Acceptance: repeated deaths do not duplicate respawns; no player respawns outside the arena or into an unsafe position.
- [ ] **Add equipment/consumable rules only after state snapshot/restore is proven.** Decide which flasks/items/spells are restricted or restored; snapshot exact values before modifying them.
  - Acceptance: after normal finish, cancel, disconnect, and failed load, player values match the intended pre-match/arena policy.
- [ ] **Add arena rotation, spectator flow, tournaments, and custom arenas as separate later milestones.** Each requires an individual rules/data plan and compatibility review.
  - Acceptance: no optional feature is enabled by default before its own host/client validation passes.

## Phase 9 — Packaging, documentation, and release readiness

- [ ] **Make the ME3 profile installable without hard-coded machine paths.** Include the arena DLL path and a documented way to configure the installed Seamless DLL path while preserving other packages/natives.
  - Acceptance: clean test profile launches the game with both mods; disabling SeamlessArena does not remove other mods.
- [ ] **Package only required files.** Include DLL, profile/config example, license, third-party notices, compatibility matrix, install/removal steps, and logs/support instructions.
  - Acceptance: install and uninstall from a clean profile follow documented steps without altering vanilla files or saves.
- [ ] **Update README and roadmap to actual status.** Label prototype behavior and unsupported features clearly; remove promotional feature claims that are not implemented.
  - Acceptance: every listed feature links to implemented code or is labeled planned.
- [ ] **Add contributor onboarding.** Document build prerequisites, project layout, how to reproduce the two-player smoke test, code formatting, and how to submit focused changes.
  - Acceptance: a second developer can build the solution and understand the next open milestone from the docs alone.
- [ ] **Run release validation for the pinned version matrix.** Build Release from a clean checkout, run Core tests, then perform the documented host/guest scenarios.
  - Acceptance: release record includes build output, test output, game/ME3/Seamless versions, and pass/fail for every scenario.

## First milestone completion criteria

The project is ready to call its first playable prototype only when all are true:

- [ ] The supported game, Seamless Co-op, and ME3 versions are exact and recorded.
- [ ] Unsupported versions stop before hook installation; startup status is accurate.
- [ ] ME3 loads the DLL alongside the pinned Seamless build.
- [ ] Two Seamless peers can enter one verified vanilla Colosseum and return together.
- [ ] The mod can identify the same two peers on host and guest.
- [ ] One Duel can start, end on first death, show the same result to both players, and cleanly return.
- [ ] Disconnect, failed transition, and mod shutdown do not strand players or leave hooks active.
- [ ] Core tests and the two-player manual scenario have recorded passing results.
