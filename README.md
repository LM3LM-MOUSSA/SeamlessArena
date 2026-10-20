# SeamlessArena — Alpha Release

**SeamlessArena** is an Elden Ring native PvP extension designed to provide a seamless, dedicated Colosseum duel experience inside [Seamless Co-op](https://github.com/yuiamoroll/EldenRingSeamlessCoopRelease) sessions via [Mod Engine 3 (ME3)](https://github.com/garyttierney/me3).

---

##  Alpha Milestone Feature Scope

The Alpha release delivers a focused, highly stable **1v1 Duel Vertical Slice**:

1. **1v1 Duel Game Mode**:
   - Strictly 2 participants (Host + Guest).
   - First death ends the match (clean win/loss attribution).
   - Deterministic tie handling if the match timer expires.
   - Suppresses vanilla death screens/penalties during duel rounds.
   - Resettable state machine that returns both players to Safe Idle without stranding.

2. **In-Game Duel Item (Duelist's Furled Finger - Goods ID 108)**:
   - Uses the vanilla **Duelist's Furled Finger** (`Goods ID: 108`, configurable via `SeamlessArena.toml`).
   - Requires **zero `regulation.bin` editing**: players already have the item in their inventory with valid icons and animations.
   - Using the item opens the duel lobby and flags the player as Ready.
   - Once both host and guest confirm with the item, the match transitions through ReadyCheck → AwaitingArena → 3-2-1 Countdown → Active Combat.

3. **Pinned Colosseum Arena**:
   - Exclusively pinned to the full-detail **Limgrave Colosseum** (`m45_02_00_00`) to guarantee high collision safety and stable scene transitions.
   - Map entry/exit tracking with loading screen pointer safety (clears cached entity pointers during zone transitions).

4. **Duelist Flask Restrictions**:
   - **0 Crimson Flasks**: Crimson flask healing is strictly restricted for competitive duel integrity.
   - **Max 3 Cerulean Flasks**: Limited FP recovery.
   - Configurable rules loaded from `SeamlessArena.toml`.

5. **Fail-Closed Dual-Native Loader Integration**:
   - Loaded by Mod Engine 3 as an x64 native DLL alongside `ersc.dll`.
   - `VersionGuard` checks exact PE `TimeDateStamp` (`0x69E9C9B9`) and `SizeOfImage` (`0x05E0A000`) for Elden Ring App v1.16/v1.16.1.
   - Transitions cleanly to **Safe Idle** if game version or memory signatures are unverified, preventing crashes.

6. **Network Synchronization Protocol**:
   - Thread-safe binary command queue for peer synchronization (`MSG_LOBBY_STATE`, `MSG_PLAYER_READY`, `MSG_DEATH_REPORT`, `MSG_MATCH_END`, `MSG_ABORT_MATCH`).
   - Host-authoritative match state with monotonic time advancement.

---

##  Architecture & Project Structure

The project is structured into modular Visual Studio / CMake C++20 components:

```text
f:\dev\SeamlessArena\
├── SeamlessArena.sln                 # Visual Studio 2026 solution (x64)
├── SeamlessArena.me3                 # Mod Engine 3 profile definition
├── SeamlessArena.toml                # User-facing runtime configuration
├── costum regs/                      # Custom regulation data (regulation.bin)
│   └── regulation.bin                # Goods item row (ID 220000)
├── bin/x64/Release/                  # Build outputs
│   ├── SeamlessArena.dll             # Native ME3 extension DLL
│   ├── SeamlessArena.Core.lib        # Core static rules library
│   └── SeamlessArena.Tests.exe       # Standalone test runner (18 passing tests)
└── src/
    ├── SeamlessArena.Core/           # Pure, game-independent PvP rules engine
    │   ├── include/SeamlessArena/Core/MatchRules.h
    │   └── src/MatchRules.cpp
    ├── SeamlessArena.Native/         # Game-side orchestrator & engine integration
    │   ├── ArenaCoordinator.h/.cpp   # Flow controller, 1v1 duel & item state
    │   ├── ArenaGameTask.h/.cpp      # FromSoftware CSTaskGroup::FrameBegin worker
    │   ├── ArenaNetProtocol.h/.cpp   # Binary wire protocol & command queue
    │   ├── ConfigLoader.h/.cpp       # TOML configuration parser
    │   ├── EngineBridge.h/.cpp       # Strongly-typed libER memory safety wrappers
    │   ├── HookManager.h/.cpp        # MinHook detour manager & item hook
    │   ├── Logger.h/.cpp             # Thread-safe diagnostic logging
    │   ├── VersionGuard.h/.cpp       # Fail-closed PE header validator
    │   └── me3_entry.cpp             # Exported C-ABI entry points for ME3
    └── SeamlessArena.Tests/          # Unit test suite
        └── main.cpp
```

---

## ⚙️ Configuration (`SeamlessArena.toml`)

```toml
[general]
log_level = "info"
auto_check_updates = false

[item]
# Goods row ID for initiating the duel (108 = Duelist's Furled Finger)
duel_goods_id = 108

[rules.duel]
duration_seconds = 300
max_crimson_flasks = 0
max_cerulean_flasks = 3
restore_consumables = false
auto_physick = false

[arena]
default_colosseum = "limgrave"
allow_spirit_ashes = false
```

---

##  Building & Validation

### Prerequisites
- Windows 10/11 x64
- Visual Studio 2026 with the C++ Desktop workload (MSVC v145 toolset)
- Windows SDK (10.0.19041.0 or newer)
- Mod Engine 3 (`me3 >= 0.12.1`)

### Build with MSBuild
```powershell
# Build Release x64
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" SeamlessArena.sln /p:Configuration=Release /p:Platform=x64
```

### Run Automated Unit Tests (18 tests)
```powershell
& "bin\x64\Release\SeamlessArena.Tests.exe"
```

Expected output:
```text
=================================================
SeamlessArena.Core Automated Test Suite (CI Gate)
=================================================
[RUN ] Test_Duel_WinCondition...                    [PASS]
[RUN ] Test_Duel_InvalidPlayerCount...              [PASS]
[RUN ] Test_Duel_TimeoutDraw...                     [PASS]
[RUN ] Test_Duel_DisconnectForfeit...               [PASS]
[RUN ] Test_Duel_SuicideOrEnvironment...            [PASS]
[RUN ] Test_DeathWhileDead_Rejected...              [PASS]
[RUN ] Test_StaleDeathAfterComplete_Rejected...     [PASS]
[RUN ] Test_DuplicatePlayerIDs_Rejected...          [PASS]
[RUN ] Test_DisconnectedPlayerAtStart_Rejected...   [PASS]
[RUN ] Test_UnitedCombat_UnbalancedTeams_Rejected...[PASS]
[RUN ] Test_UnitedCombat_TeamDisconnect...          [PASS]
[RUN ] Test_FriendlyFire_NoScore...                 [PASS]
[RUN ] Test_CombatOrdeal_ScoreCapAndRespawn...      [PASS]
[RUN ] Test_ScoreLimitZero_TimedOnly...             [PASS]
[RUN ] Test_TimerAdvance_And_Draw...                [PASS]
[RUN ] Test_TimerOverrun_ClampedToZero...           [PASS]
[RUN ] Test_MatchCancellation...                    [PASS]
[RUN ] Test_ResetAndRestart...                      [PASS]
=================================================
Test Summary: 18 Passed, 0 Failed
=================================================
```

---

##  Installation with ME3 & Seamless Co-op

1. Ensure **Elden Ring** (v1.16/v1.16.1) and **Seamless Co-op** (`ersc.dll`) are installed.
2. In `SeamlessArena.me3`, confirm that the path to `ersc.dll` matches your Seamless Co-op installation.
3. Launch Elden Ring through ME3:
   ```powershell
   me3 launch --file "SeamlessArena.me3"
   ```
4. Check `SeamlessArena.log` to confirm initialization status:
   ```text
   [Bootstrap] SeamlessArena Native Extension initializing...
   [VersionGuard] Elden Ring build verified (Timestamp: 0x69E9C9B9, Size: 0x05E0A000)
   [VersionGuard] Seamless Co-op (ersc.dll) detected at base 0x...
   [ArenaCoordinator] ArenaCoordinator initialized. Phase: Idle. Duel Item ID: 108, Arena: Limgrave, MaxCrimson: 0, MaxCerulean: 3
   ```
5. **How to Use & Test the Match Flow**:
   - **Solo Testing (No friend needed)**:
     1. Use the vanilla **Duelist's Furled Finger** (Goods ID: `108`) from your inventory.
     2. Player 1 is immediately warped to **Limgrave Colosseum** (`m45_02_00_00`) and placed at South Spawn in `WaitingInArena` state.
     3. You can explore and wait in the arena safely for an opponent to join.
     4. If you want to test the full combat loop alone right now, use the **Duelist's Furled Finger** again while waiting in the arena to start **Solo Practice Mode** (spawns a virtual sparring partner, triggering the 3-2-1 countdown, flask rules, and combat loop).
   - **With a Friend (1v1 Duel)**:
     1. Player 1 uses the **Duelist's Furled Finger** to enter the Colosseum.
     2. Player 2 joins the session and uses their **Duelist's Furled Finger**.
     3. Once both are ready in the Colosseum, the 3-2-1 countdown chime sounds and the duel begins! First death ends the match with immediate winner broadcast.

---

##  License & Credits

- SeamlessArena is distributed under the MIT License.
- Seamless Co-op is created and maintained by [LukeYui](https://github.com/yuiamoroll/EldenRingSeamlessCoopRelease).
- Mod Engine 3 is created by [garyttierney](https://github.com/garyttierney/me3).
- Built with [libER](https://github.com/Dasaav-dsv/libER) and [MinHook](https://github.com/TsudaKageworthy/minhook).
