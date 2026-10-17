# 05 — Vanilla Colosseum Map Investigation & Transitions

## 1. Objective

To provide an arena PvP experience without bundling large custom map files, SeamlessArena leverages Elden Ring's three built-in Colosseums introduced in the Free Colosseum Update:
1. **Royal Colosseum (Leyndell)**
2. **Caelid Colosseum**
3. **Limgrave Colosseum**

This document establishes the verified map identifiers, coordinates, scene transition mechanics, and spawn positions.

---

## 2. Colosseum Map Identifiers & Structure

According to Souls Modding Wiki and Smithbox MSB data, the internal map IDs are structured as follows:

| Map ID | Arena Name | In-Game Location | Description |
|---|---|---|---|
| `m45_00_00_00` | **Royal Colosseum** | Leyndell, Capital City | Combat arena MSB with circular perimeter and spectator stands. |
| `m45_01_00_00` | **Caelid Colosseum** | Isolated Great-Jar cliff, Caelid | Arena featuring pillars and elevation variance; supports Spirit Ashes in vanilla. |
| `m45_02_00_00` | **Limgrave Colosseum** | Northern storm cliffs, Limgrave | Open stone floor, classic duel pit with clear sightlines. |
| `m45_00_00_99` | Royal Colosseum (Overworld) | Leyndell exterior | Exterior shell used when player is standing in overworld map `m60`. |
| `m45_01_00_99` | Caelid Colosseum (Overworld) | Caelid exterior | Exterior shell for overworld rendering. |
| `m45_02_00_99` | Limgrave Colosseum (Overworld) | Limgrave exterior | Exterior shell for overworld rendering. |

### 2.1. The `00` vs `99` Map Role
- **Combat Arenas (`_00_00`):** These are standalone scene maps containing the playable collision geometry, invisible barrier walls (`hkpRigidBody`), spectator boundary zones, player spawn points (`MSB Region`), and event triggers.
- **Overworld Shells (`_00_99`):** These are low-detail / exterior geometry components loaded alongside overworld blocks (`m60_XX_XX_XX`). They lack player spawn regions and combat collision barriers.
- **Conclusion:** **SeamlessArena must warp players into `m45_00_00_00`, `m45_01_00_00`, or `m45_02_00_00` for all matches.**

---

## 3. Spawn Coordinates & Arena Topology

The arenas have distinct spatial layouts suitable for specific player counts:

```text
               [North Spawn - Team 2 / Guest]
                            |
                     15m - 20m Separation
                            |
                        [Center]  <--- Center Pillar (Caelid only)
                            |
                     15m - 20m Separation
                            |
               [South Spawn - Team 1 / Host]
```

### 3.1. Standard Spawn Allocation
- **Duel (1v1):** 
  - Host / Player 1: South Spawn point facing North (`Yaw = 0.0 rad`).
  - Guest / Player 2: North Spawn point facing South (`Yaw = 3.14159 rad`).
- **Combat Ordeal (FFA 2–6):**
  - Up to 6 radial spawn points distributed around the outer rim of the arena floor, separated by at least 15 meters to prevent immediate spawn-camping.
- **United Combat (Teams):**
  - Team 1: South cluster (3 positions spaced 3 meters apart).
  - Team 2: North cluster (3 positions spaced 3 meters apart).

---

## 4. Scene Transition Mechanics

### 4.1. The Native Warp Mechanism
To transition the party into the Colosseum, SeamlessArena triggers the engine's internal map warp function via `CS::FieldArea` or `CS::WorldChrMan`:

```cpp
struct WarpRequest {
    uint32_t mapId;         // e.g. 45020000 for Limgrave Colosseum
    float posX, posY, posZ; // Target world coordinates
    float yaw;              // Orientation angle
};

void WarpPartyToArena(uint32_t arenaMapId) {
    // 1. Host issues warp command to local engine
    // 2. Seamless Co-op party packet transmits target map ID to all guests
    // 3. Engine handles black-fade, asset loading, and entity relocation
}
```

### 4.2. Safe Return to World
When the match concludes or is cancelled:
1. SeamlessArena reads the player's saved Overworld coordinates or defaults to the last visited Site of Grace (e.g., Roundtable Hold `m11_10_00_00` or the Colosseum entrance door).
2. The party is safely warped out of the combat arena before clearing match state.
3. This guarantees no players remain stranded inside closed Colosseum collision meshes.
