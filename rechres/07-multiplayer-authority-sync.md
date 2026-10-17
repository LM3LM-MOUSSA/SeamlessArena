# 07 — Multiplayer Authority, Network Synchronization & Wire Protocol

## 1. Network Topology and Authority Model

SeamlessArena adopts a **Host-Authoritative / Client-Replicated** networking model matching Seamless Co-op's session structure:

- **Host (Player 1):** 
  - Owns the `MatchRules` state machine instance.
  - Controls match phase transitions (Lobby, Countdown, Active, Complete).
  - Validates kill events and manages the official round timer.
  - Broadcasts authoritative match snapshots to all guest peers.
- **Guest Clients (Peers):**
  - Maintain a read-only mirror of the `MatchSnapshot`.
  - Transmit local input intents (Ready toggle, Mode vote) and local death notices to the Host.
  - Render HUD elements and trigger local banners based on Host notifications.

---

## 2. Wire Protocol Specification

Messages are serialized as compact binary packets with a fixed 4-byte header:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Magic (0x5341 = 'SA')   |    Opcode     |  Seq Number   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Data...                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 2.1. Packet Opcodes

| Opcode | Identifier | Direction | Description |
|---|---|---|---|
| `0x01` | `MSG_LOBBY_STATE` | Host -> Peers | Broadcasts current lobby settings, arena ID, and player slots. |
| `0x02` | `MSG_PLAYER_READY` | Peer -> Host | Peer signals ready/unready status. |
| `0x03` | `MSG_COUNTDOWN_START` | Host -> Peers | Commands peers to freeze positions and display countdown. |
| `0x04` | `MSG_MATCH_START` | Host -> Peers | Signals combat active; unfreezes inputs and starts timers. |
| `0x05` | `MSG_DEATH_REPORT` | Peer -> Host | Informs host that local character reached 0 HP. |
| `0x06` | `MSG_SCORE_SYNC` | Host -> Peers | Authoritative score update and remaining duration broadcast. |
| `0x07` | `MSG_MATCH_END` | Host -> Peers | Announces winner ID / winning team and displays results. |
| `0x08` | `MSG_ABORT_MATCH` | Either -> All | Immediately terminates match due to disconnect or error. |

---

## 3. Packet Payloads & Serialization

```cpp
#pragma pack(push, 1)

struct PacketHeader {
    uint16_t magic{ 0x5341 }; // 'SA'
    uint8_t opcode;
    uint8_t sequenceNumber;
};

struct MsgDeathReport {
    PacketHeader header{ 0x5341, 0x05, 0 };
    uint64_t victimSteamId;
    uint64_t killerSteamId; // 0 if environmental / self
};

struct MsgScoreSync {
    PacketHeader header{ 0x5341, 0x06, 0 };
    uint32_t secondsRemaining;
    uint8_t playerCount;
    struct PlayerEntry {
        uint64_t steamId;
        uint16_t score;
        uint16_t deaths;
        uint8_t alive;
    } players[6];
};

#pragma pack(pop)
```

---

## 4. Desync Detection & Conflict Resolution

1. **Host Arbitration:** If a peer reports a kill on Player B, but Player B's client claims they survived with 1 HP due to roll latency, the **Host's verification is definitive**. The host evaluates health state using the engine's authoritative damage synchronization.
2. **Heartbeat & Sequence Validation:**
   - Host sends `MSG_SCORE_SYNC` every 1.0 second.
   - If a peer does not receive a sync packet for 5.0 seconds, the peer displays a "Sync Lost" warning on screen.
   - If connection cannot be re-established within 10 seconds, the client gracefully aborts back to the overworld hub.
3. **Preventing Replay / Duplicate Deaths:**
   - The host tracks a `hasDiedThisLife` boolean for each player. Duplicate death packets within the respawn window are dropped.
