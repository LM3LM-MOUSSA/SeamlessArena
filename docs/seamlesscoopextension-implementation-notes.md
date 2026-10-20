# SeamlessCoopExtension review and SeamlessArena changes

## Reference review

The downloaded project is under `docs/SeamlessCoopExtension-main`. Its current C++ experiment is in `dllscripting main- clion/maincoopextension`; the repository README says the older Visual Studio project is outdated. The Cheat Engine and ModEngine folders contain experiments for a custom death/respawn flow. The current C++ code explores signature scans, player-memory access, damage observation, a local hit tally, and timed direct writes to health, animation, and coordinates.

The source is useful as a list of behaviors to investigate. It does not implement SeamlessArena's match transport, stable multiplayer identity mapping, host authority, arena travel, match result synchronization, or a production respawn system. The C++ sample also contains unverified offsets/signatures and failure paths that must not be copied into the shipped DLL.

The downloaded repository has no root `LICENSE` file. SeamlessArena therefore uses a clean-room implementation of the relevant design ideas; it does not copy source from that repository. Third-party files bundled inside the download retain their own license notices and are not incorporated here.

## Implementations made

### Frame-time accumulation

`ArenaCoordinator::Tick` previously rounded every positive frame delta up to one second. At 60 frames per second, that could consume about 60 seconds of match time per real second. The coordinator now rejects non-finite or negative deltas, caps long stalls, accumulates fractional frame time, and advances match rules once per accumulated second.

### Participant validation

The Duel startup path previously substituted IDs `1` and `2` when real player IDs were missing. It also initialized `PlayerState` with an aggregate that set `connected` to false. Startup now requires two distinct nonzero IDs and creates both match participants as connected and alive. Lobby creation requires a registered host, a positive duration, and a valid Colosseum selection. Duplicate or zero remote IDs are ignored.

### Damage-to-kill attribution boundary

`ArenaCoordinator::OnDamageApplied` now records a hit only during an active match, for positive finite damage between two connected match participants. PvE damage, self-damage, missing IDs, and non-participants are ignored. The record is keyed by stable match participant ID rather than a raw character pointer.

`OnPlayerDeath` first validates the victim and any supplied killer against the match. If a verified death callback has no valid killer, it may credit the latest eligible participant hit within a 10-second window. Local HP polling now reports only the victim; it no longer assumes the guest caused the death.

The game damage hook still cannot call this method: `Hook_DamageAttribution` is intentionally fail-closed, and the project has not yet verified how to map `ChrIns*` to stable Seamless participant IDs. That engine integration remains required before kill credit can work in game.

### Arena-entry phase and truthful cleanup

Ready players now enter `AwaitingArena`; the countdown starts only after `OnArenaSceneLoaded` is called. This avoids starting combat while still in the overworld. The existing hook is still a stub, and no verified warp command is connected, so players are not moved automatically.

Cleanup now waits for an overworld-load notification instead of immediately claiming that the party returned. Spawn placement, respawn, inventory restoration, and overworld warp methods log that they are unavailable rather than claiming success. The reference's hardcoded respawn coordinates, effect IDs, event flag, and animation values are not treated as verified SeamlessArena implementation data.

## Still required before a playable match

1. Verify the supported Elden Ring build's runtime map-ID representation and full-detail Colosseum scene-load callback.
2. Connect the travel operation and require successful scene confirmation before countdown.
3. Resolve stable participant IDs from game characters using a verified Seamless integration boundary.
4. Find and validate a damage/death callback for the supported game build; wire its participant-resolved events to `OnDamageApplied` and `OnPlayerDeath`.
5. Determine whether Seamless exposes a supported custom-message route. The current `ArenaNetProtocol` remains a stub; this reference project does not supply that transport.
6. Implement and verify safe death handling, respawn, inventory snapshots/restoration, disconnect handling, and recovery before enabling vanilla-death suppression.

The current changes improve the coordinator's correctness, but they do not make the native DLL playable or enable any unverified game hook.
