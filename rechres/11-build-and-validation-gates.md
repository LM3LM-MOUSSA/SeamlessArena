# 11 — Build Pipeline, Tooling & In-Game Validation Gates

## 1. Toolchain & Solution Architecture

SeamlessArena is developed using Microsoft Visual Studio 2026 targeting x86_64 Windows:
- **Solution File:** `SeamlessArena.sln`
- **Compiler Toolset:** Visual Studio 2026 (`v145`) or Visual Studio 2022 (`v143`)
- **C++ Standard:** ISO C++20 (`/std:c++20`)
- **Warning Level:** `/W4` with `/WX` (treat warnings as errors in Core)
- **Runtime Library:** Multi-threaded Static (`/MT` in Release, `/MTd` in Debug) to avoid external VC++ runtime DLL dependencies.

### 1.1. Project Structure
```text
SeamlessArena/
├── SeamlessArena.sln
├── src/
│   ├── SeamlessArena.Core/          (Static Library: Pure C++20 rules)
│   │   ├── include/SeamlessArena/Core/MatchRules.h
│   │   └── src/MatchRules.cpp
│   └── SeamlessArena.Native/        (Dynamic Link Library: ME3 / Hooks)
│       ├── me3_entry.cpp
│       └── dllmain.cpp
```

---

## 2. Automated Testing for `SeamlessArena.Core`

Because `SeamlessArena.Core` has no dependencies on Elden Ring, DirectX, or Windows hooks, its rules can be tested via unit tests running in continuous integration:

```cpp
// Example unit test for Duel lifecycle
void Test_Duel_WinCondition() {
    using namespace seamless_arena::core;
    
    MatchRules rules;
    MatchConfig config{ MatchMode::Duel, 300, 1 };
    std::vector<PlayerState> players{
        { 1001, 1, 0, 0, true, true },
        { 1002, 2, 0, 0, true, true }
    };
    
    std::string err;
    assert(rules.Start(config, players, err) == true);
    assert(rules.Snapshot().phase == MatchPhase::Active);

    // Simulate Player 1002 dying
    assert(rules.RecordDeath(1002, 1001) == true);

    auto snap = rules.Snapshot();
    assert(snap.phase == MatchPhase::Complete);
    assert(snap.winningPlayer.value() == 1001);
}
```

---

## 3. In-Game Validation Gates

Before publishing releases, each build must pass three sequential validation gates:

### Gate 1: Loader & Version Smoke Test (Single Player)
1. Launch game with ME3 profile containing both `ersc.dll` (`load_early = true`) and `SeamlessArena.Native.dll`.
2. Inspect `SeamlessArena.log` to confirm:
   - `SeamlessArena_Initialize` invoked without errors.
   - Elden Ring executable timestamp recognized.
   - `ersc.dll` base address located.
   - No crash or abnormal memory spike during main menu load.

### Gate 2: Colosseum Scene Transition & Recovery (Host Only)
1. Load into a local save at Roundtable Hold.
2. Trigger the debug transition command into `m45_02_00_00` (Limgrave Colosseum).
3. Confirm player spawns cleanly on the arena floor without clipping through geometry.
4. Trigger the return-to-world command and verify the player returns to Roundtable Hold without losing runes or inventory.

### Gate 3: Two-Player Duel Vertical Slice (Host + Peer)
1. Host opens a Seamless Co-op party session.
2. Guest peer joins the session via Effigy.
3. Host initiates Duel mode in Limgrave Colosseum.
4. Verify:
   - Both players warp into the arena and freeze during the 3-second countdown.
   - Inputs unfreeze on "Fight!".
   - Combat damage registers normally over Seamless networking.
   - When Peer dies, Host receives death event immediately.
   - Both screens display the correct "VICTORY" / "DEFEAT" banners.
   - Both players return safely to the overworld lobby.
