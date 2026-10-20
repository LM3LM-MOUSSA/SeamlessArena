#pragma once
// =============================================================================
// ArenaNetProtocol.h — Wire protocol for host-peer arena message exchange
// =============================================================================
// RESEARCH: rechres/07-multiplayer-authority-sync.md (ALL sections)
// RESEARCH: rechres/02-seamless-coop-integration.md  (Section 2.2 — Steam sockets)
//
// Defines the binary packet format, opcodes, serialization, and the
// thread-safe command queue for delivering network events to the game thread.
//
// Network I/O happens on a background thread (Steam socket callback).
// Received packets are deserialized and pushed into a lock-free queue.
// The ArenaCoordinator's Tick() drains this queue on the game thread.
// =============================================================================

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Packet header (4 bytes)
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 2)
    // -------------------------------------------------------------------------
    static constexpr std::uint16_t kPacketMagic = 0x5341; // 'SA'

    #pragma pack(push, 1)

    struct PacketHeader
    {
        std::uint16_t magic = kPacketMagic;
        std::uint8_t  opcode = 0;
        std::uint8_t  sequenceNumber = 0;
    };

    // -------------------------------------------------------------------------
    // Opcodes
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 2.1)
    // -------------------------------------------------------------------------
    namespace Opcode
    {
        constexpr std::uint8_t MSG_LOBBY_STATE     = 0x01; // Host -> Peers
        constexpr std::uint8_t MSG_PLAYER_READY    = 0x02; // Peer -> Host
        constexpr std::uint8_t MSG_COUNTDOWN_START = 0x03; // Host -> Peers
        constexpr std::uint8_t MSG_MATCH_START     = 0x04; // Host -> Peers
        constexpr std::uint8_t MSG_DEATH_REPORT    = 0x05; // Peer -> Host
        constexpr std::uint8_t MSG_SCORE_SYNC      = 0x06; // Host -> Peers
        constexpr std::uint8_t MSG_MATCH_END       = 0x07; // Host -> Peers
        constexpr std::uint8_t MSG_ABORT_MATCH     = 0x08; // Either -> All
    }

    // -------------------------------------------------------------------------
    // Packet payloads
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 3)
    // -------------------------------------------------------------------------

    struct MsgLobbyState
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_LOBBY_STATE, 0 };
        std::uint8_t  mode;              // MatchMode enum value
        std::uint8_t  arenaId;           // ColosseumId enum value
        std::uint32_t durationSeconds;
        std::uint16_t scoreLimit;
        std::uint8_t  playerCount;
        std::uint64_t playerSteamIds[6]; // up to 6 players
        std::uint8_t  playerTeams[6];
    };

    struct MsgPlayerReady
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_PLAYER_READY, 0 };
        std::uint64_t steamId;
        std::uint8_t  isReady; // 1 = ready, 0 = unready
    };

    struct MsgDeathReport
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_DEATH_REPORT, 0 };
        std::uint64_t victimSteamId;
        std::uint64_t killerSteamId; // 0 if environmental / suicide
    };

    struct MsgScoreSync
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_SCORE_SYNC, 0 };
        std::uint32_t secondsRemaining;
        std::uint8_t  playerCount;
        struct PlayerEntry
        {
            std::uint64_t steamId;
            std::uint16_t score;
            std::uint16_t deaths;
            std::uint8_t  alive; // 1 = alive, 0 = dead
        } players[6];
    };

    struct MsgMatchEnd
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_MATCH_END, 0 };
        std::uint64_t winnerSteamId;    // 0 if team win or draw
        std::uint8_t  winnerTeam;       // 0xFF if individual win or draw
        std::uint8_t  isDraw;           // 1 = draw
    };

    struct MsgAbortMatch
    {
        PacketHeader header = { kPacketMagic, Opcode::MSG_ABORT_MATCH, 0 };
        std::uint64_t senderSteamId;
    };

    #pragma pack(pop)

    // -------------------------------------------------------------------------
    // Deserialized network command (queued for game thread)
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 4)
    // -------------------------------------------------------------------------
    struct NetCommand
    {
        std::uint8_t  opcode = 0;
        std::uint64_t senderSteamId = 0;

        // Payload fields per opcode
        std::uint64_t victimId = 0;
        std::uint64_t killerId = 0;
        std::uint8_t  readyFlag = 0;
        std::uint64_t winnerId = 0;
        bool          isDraw = false;
        std::uint8_t  mode = 0;
        std::uint8_t  arenaId = 0;
        std::uint32_t durationSeconds = 0;
        std::uint16_t scoreLimit = 0;
    };

    // =========================================================================
    // ArenaNetProtocol — manages send/receive of arena packets
    // =========================================================================
    class ArenaNetProtocol
    {
    public:
        bool Initialize();
        void Shutdown();

        // -- Sending ----------------------------------------------------------
        void SendToHost(const void* packetData, std::size_t packetSize);
        void BroadcastToAllPeers(const void* packetData, std::size_t packetSize);
        void SendToPeer(std::uint64_t targetSteamId, const void* packetData, std::size_t packetSize);

        // -- High-level broadcast helpers -------------------------------------
        void BroadcastPlayerReady(std::uint64_t steamId, bool isReady);
        void BroadcastLobbyState(const MsgLobbyState& msg);
        void BroadcastDeathReport(std::uint64_t victimId, std::uint64_t killerId);
        void BroadcastMatchEnd(std::uint64_t winnerId, bool isDraw);
        void BroadcastAbortMatch(std::uint64_t senderId);

        // -- Receiving --------------------------------------------------------
        void ProcessPacket(const void* packetData, std::size_t packetSize, std::uint64_t senderSteamId);
        void EnqueueCommand(const NetCommand& cmd);
        std::optional<NetCommand> DequeueCommand();

    private:
        std::uint8_t nextSequence_ = 0;
        std::uint8_t lastReceivedSequence_ = 0;

        mutable std::mutex queueMutex_;
        std::deque<NetCommand> commandQueue_;
    };

} // namespace seamless_arena::native
