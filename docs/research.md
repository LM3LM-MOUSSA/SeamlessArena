# Research findings

Research date: 16 October 2026. Game assumption: Elden Ring on Windows/Steam. The user confirmed that “BetterPvP” means Elden Ring Better Multiplayer / DEN Maps.

## Vanilla arena system

Bandai Namco describes three Colosseums—Limgrave, Leyndell/Royal, and Caelid—with arena-specific rules. The mode rules that define the initial scope are:

- **Duel:** two players, no respawn, first death ends the match.
- **Combat Ordeal:** free-for-all, respawns allowed, points scored until the timer expires.
- **United Combat:** teams, respawns allowed, timed score-based match.
- **Caelid variant:** supports the above formats with Spirit Ashes. Keep this optional and out of the first PvP prototype until its multiplayer synchronization is understood.

These are the mode semantics to preserve. The first version should use existing game locations rather than ship new map assets.

## Seamless Co-op constraints

The current Seamless Co-op guide describes a persistent party session controlled by a host, a six-player party, a shared game/mod version requirement, Steam networking, and a save format separate from vanilla saves. Seamless Co-op expects an early Mod Engine extension handoff (`modengine_ext_init`). ME3 added support for mods that rely on this handoff; a ME3/Seamless issue reports Elden Ring + Seamless working with that support. This loader capability is not proof that this add-on's runtime integration is correct.

The current ME3 profile format uses `[[natives]]` with a DLL `path`; paths are relative to the `.me3` file. It supports a native `initializer` function and a `load_early` flag. Seamless Co-op should be listed as a native with `load_early = true`; the add-on can then be loaded by the same ME3 profile. ME3 blocks official matchmaking by default, which is appropriate for this Seamless-only add-on; `start_online` is not needed for Seamless.

Therefore:

1. Treat game and Seamless updates as compatibility breaks until retested.
2. Keep all game-specific hooks behind one adapter layer; do not spread raw game pointers through the arena rules.
3. Prototype host-to-party synchronization before implementing scoreboards or respawn logic.
4. Require each party member to use the same add-on version and match settings.
5. Keep this for Seamless/modded sessions; do not target official matchmaking.

The Seamless multi-mod guide is community documentation and may lag current releases. Recheck it and the current Seamless release instructions before distributing setup steps to players.

## Better Multiplayer source review

The provided source is a Rust `cdylib` with a local `waygate-client` dependency outside the supplied folder. Its `lib.rs` shows startup/shutdown order and subsystem organization that can inform our C++ design. The inspected EZState hook, player-limit hook, release-flag patch, and file processor are mostly state flags and log messages; they do not show implemented game hooks in those files. The Rust implementation cannot be included in this C++-only solution, and feature comments should not be mistaken for verified working behavior.

## ME3 loader and Seamless Co-op

The solution includes `SeamlessArena.me3`, which supports Elden Ring, loads the Release DLL, and names its exported initializer. To use a single profile with Seamless Co-op, add a second native entry for the installed `ersc.dll` and set `load_early = true` on that entry. Paths resolve from the profile file. The scaffold profile deliberately does not guess where Seamless is installed.

Add an entry like this, replacing the example path with the path to your installed Seamless DLL relative to `SeamlessArena.me3`:

```toml
[[natives]]
path = "PATH/TO/SeamlessCoop/ersc.dll"
load_early = true
```

Use a current ME3 release. ME3 v0.5.0 records support for `modengine_ext_init`, and a later issue response recommended updating ME3 and setting `load_early = true` for Seamless. ME3 v0.13.0 is the latest release verified during this research (11 August 2026). The native initializer in this project is a small C ABI readiness callback; it does not install hooks yet.

## Better Multiplayer / DEN Maps reference

The current Elden Ring reference is Better Multiplayer, described by its authors as the successor to DEN Maps. The product patterns that fit this project are dedicated PvP areas, tournament-friendly match setup, visible teams, useful respawn rules, consumable restoration, automatic pre-match Physick, anti-spawn-camp protection, and variants for 2v2/3v3.

For this project, borrow those *product ideas* only. Better Multiplayer’s Nexus permissions prohibit reuse of its assets; do not copy its code, map files, labels, or graphics. Create original implementation and assets, and use the vanilla Colosseum scenes first.

## Visual Studio 2026 target

Microsoft documents Visual Studio 2026 C++ MSBuild projects using the v145 platform toolset. The solution is therefore an x64 MSBuild `.sln` / `.vcxproj` project, with C++20 selected for broad compiler support.

## Open technical questions

- Can a Seamless party reliably transition together into the vanilla Colosseum scenes without relying on official Colosseum matchmaking?
- Which game-side events expose player death, round start/end, score, team, and respawn state in a way that is safe to hook?
- How does Seamless synchronize relevant player and world state, and can this add-on publish additional state without conflicting with it?
- What is the most stable in-game entry point: an existing arena interaction, a Seamless-compatible item, or a small native UI?
- Do all target players need Shadow of the Erdtree, and what precise game build should be supported first?

These questions are the feasibility gate before implementing match rules.

## Sources

- Bandai Namco, [Free Colosseum Update](https://en.bandainamcoent.eu/elden-ring/news/elden-ring-free-colosseum-update-available-now)
- Seamless Co-op, [Co-op overview and current download page](https://seamlesscoop.net/coop/)
- Seamless Co-op documentation, [Using Seamless with other mods](https://ersc-docs.github.io/seamless-modding/)
- Seamless Co-op documentation, [Install and update](https://ersc-docs.github.io/how-to-install-and-update/)
- ME3, [creating mod profiles](https://github.com/garyttierney/me3/blob/main/docs/user-guide/creating-mod-profiles.md) and [configuration reference](https://github.com/garyttierney/me3/blob/main/docs/configuration-reference.md)
- ME3, [release history](https://github.com/garyttierney/me3/releases) and [Seamless compatibility discussion](https://github.com/garyttierney/me3/discussions/118)
- Seamless Co-op release mirror, [GitHub repository](https://github.com/yuiamoroll/EldenRingSeamlessCoopRelease)
- Nexus Mods, [Better Multiplayer](https://www.nexusmods.com/eldenring/mods/10171)
- Microsoft Learn, [MSVC Build Tools and Visual Studio 2026](https://learn.microsoft.com/en-us/cpp/overview/what-s-new-for-msvc?view=msvc-180)
