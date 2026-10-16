# Implementation roadmap

## Phase 0 — Research and project shell (complete)

- Confirm game, PvP-only boundary, reference mod, target IDE, and native language.
- Record vanilla Colosseum rules and Seamless Co-op constraints.
- Create the Visual Studio 2026 x64 DLL solution, ME3 profile, and exported native initializer.
- Implement the game-independent mode rules for Duel, Combat Ordeal, and United Combat in a static C++ library.

**Status:** project shell and pure rules core exist. The rules are not connected to the DLL or Elden Ring; no game hooks or playable match flow exist yet.

## Phase 1 — Compatibility prototype

1. Identify and record the exact Elden Ring, Seamless Co-op, and ME3 builds for the first target.
2. Add the installed Seamless DLL path to the profile with its early-load setting, then confirm both DLLs load together.
3. Add startup logging and a game-version check that refuses unknown builds.
4. Prove a host and party can enter and leave one existing Colosseum scene together.
5. Map player death/round events and determine whether they can be synchronized safely.
6. Connect the DLL to the rules core after safe initialization and shared state are established.

**Gate:** do not build a multiplayer scoreboard or respawn loop until arena transitions and shared match state are reliable.

## Phase 2 — Duel vertical slice

- Lobby with a ready check for exactly two players.
- Select one vanilla arena and launch a no-respawn Duel.
- Detect first death, end the round, show the winner, and return the party to the lobby.
- Add a clean cancel path for disconnects and host departure.

## Phase 3 — Vanilla respawn modes

- Add configurable timer and score limit.
- Implement Combat Ordeal free-for-all and United Combat teams.
- Add synchronized score display, round-end tie handling, and team identification.
- Add safe respawn points and temporary spawn protection to reduce spawn camping.

## Phase 4 — Better arena flow

- Add arena rotation and small-team formats (2v2 and 3v3).
- Add tournament-friendly rematches and spectator flow.
- Add optional Physick automation and consumable restoration after verifying item tracking and synchronization.
- Add host configuration checks so every player has the same mod and rules version.

## Phase 5 — Optional expansion

- Evaluate custom arenas only after the vanilla-scene implementation is stable.
- Consider rankings or progression only with an explicit design for save isolation, integrity, and party authority.
- Retest against each Elden Ring and Seamless Co-op update before declaring compatibility.

## Success criteria for the first playable release

- Two Seamless players can launch and finish a Duel repeatedly in a vanilla Colosseum.
- Both clients agree on arena, round state, and winner.
- Leaving or disconnecting cannot strand either player in the match state.
- No PvE, invasion, or vanilla save behavior is changed by the add-on.
