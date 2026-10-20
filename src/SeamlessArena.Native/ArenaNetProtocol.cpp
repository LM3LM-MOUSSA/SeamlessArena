#include "ArenaNetProtocol.h"
#include "Logger.h"

#include <cstring>

namespace seamless_arena::native
{
    bool ArenaNetProtocol::Initialize()
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.clear();
        nextSequence_ = 1;
        lastReceivedSequence_ = 0;
        SA_LOG_INFO("ArenaNetProtocol", "Network protocol initialized with thread-safe queue.");
        return true;
    }

    void ArenaNetProtocol::Shutdown()
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.clear();
        SA_LOG_INFO("ArenaNetProtocol", "Network protocol shut down.");
    }

    void ArenaNetProtocol::SendToHost(const void* packetData, std::size_t packetSize)
    {
        if (!packetData || packetSize < sizeof(PacketHeader))
            return;

        SA_LOG_DEBUG("ArenaNetProtocol", "Sending packet size %zu to host", packetSize);
        // Process local loopback for host/peer testing
        ProcessPacket(packetData, packetSize, 0);
    }

    void ArenaNetProtocol::BroadcastToAllPeers(const void* packetData, std::size_t packetSize)
    {
        if (!packetData || packetSize < sizeof(PacketHeader))
            return;

        SA_LOG_DEBUG("ArenaNetProtocol", "Broadcasting packet size %zu to peers", packetSize);
        // Process local loopback
        ProcessPacket(packetData, packetSize, 0);
    }

    void ArenaNetProtocol::SendToPeer(std::uint64_t targetSteamId, const void* packetData, std::size_t packetSize)
    {
        if (!packetData || packetSize < sizeof(PacketHeader))
            return;

        SA_LOG_DEBUG("ArenaNetProtocol", "Sending packet size %zu to peer 0x%llX", packetSize, targetSteamId);
        ProcessPacket(packetData, packetSize, targetSteamId);
    }

    void ArenaNetProtocol::BroadcastPlayerReady(std::uint64_t steamId, bool isReady)
    {
        MsgPlayerReady msg{};
        msg.header.opcode = Opcode::MSG_PLAYER_READY;
        msg.header.sequenceNumber = nextSequence_++;
        msg.steamId = steamId;
        msg.isReady = isReady ? 1 : 0;
        BroadcastToAllPeers(&msg, sizeof(msg));
    }

    void ArenaNetProtocol::BroadcastLobbyState(const MsgLobbyState& msg)
    {
        MsgLobbyState copy = msg;
        copy.header.opcode = Opcode::MSG_LOBBY_STATE;
        copy.header.sequenceNumber = nextSequence_++;
        BroadcastToAllPeers(&copy, sizeof(copy));
    }

    void ArenaNetProtocol::BroadcastDeathReport(std::uint64_t victimId, std::uint64_t killerId)
    {
        MsgDeathReport msg{};
        msg.header.opcode = Opcode::MSG_DEATH_REPORT;
        msg.header.sequenceNumber = nextSequence_++;
        msg.victimSteamId = victimId;
        msg.killerSteamId = killerId;
        BroadcastToAllPeers(&msg, sizeof(msg));
    }

    void ArenaNetProtocol::BroadcastMatchEnd(std::uint64_t winnerId, bool isDraw)
    {
        MsgMatchEnd msg{};
        msg.header.opcode = Opcode::MSG_MATCH_END;
        msg.header.sequenceNumber = nextSequence_++;
        msg.winnerSteamId = winnerId;
        msg.winnerTeam = 0xFF;
        msg.isDraw = isDraw ? 1 : 0;
        BroadcastToAllPeers(&msg, sizeof(msg));
    }

    void ArenaNetProtocol::BroadcastAbortMatch(std::uint64_t senderId)
    {
        MsgAbortMatch msg{};
        msg.header.opcode = Opcode::MSG_ABORT_MATCH;
        msg.header.sequenceNumber = nextSequence_++;
        msg.senderSteamId = senderId;
        BroadcastToAllPeers(&msg, sizeof(msg));
    }

    void ArenaNetProtocol::ProcessPacket(const void* packetData, std::size_t packetSize, std::uint64_t senderSteamId)
    {
        if (!packetData || packetSize < sizeof(PacketHeader))
            return;

        const auto* header = reinterpret_cast<const PacketHeader*>(packetData);
        if (header->magic != kPacketMagic)
        {
            SA_LOG_WARN("ArenaNetProtocol", "Packet rejected: invalid magic 0x%04X", header->magic);
            return;
        }

        NetCommand cmd{};
        cmd.opcode = header->opcode;
        cmd.senderSteamId = senderSteamId;

        switch (header->opcode)
        {
        case Opcode::MSG_PLAYER_READY:
            if (packetSize >= sizeof(MsgPlayerReady))
            {
                const auto* msg = reinterpret_cast<const MsgPlayerReady*>(packetData);
                cmd.senderSteamId = msg->steamId;
                cmd.readyFlag = msg->isReady;
                EnqueueCommand(cmd);
            }
            break;

        case Opcode::MSG_DEATH_REPORT:
            if (packetSize >= sizeof(MsgDeathReport))
            {
                const auto* msg = reinterpret_cast<const MsgDeathReport*>(packetData);
                cmd.victimId = msg->victimSteamId;
                cmd.killerId = msg->killerSteamId;
                EnqueueCommand(cmd);
            }
            break;

        case Opcode::MSG_MATCH_END:
            if (packetSize >= sizeof(MsgMatchEnd))
            {
                const auto* msg = reinterpret_cast<const MsgMatchEnd*>(packetData);
                cmd.winnerId = msg->winnerSteamId;
                cmd.isDraw = (msg->isDraw != 0);
                EnqueueCommand(cmd);
            }
            break;

        case Opcode::MSG_ABORT_MATCH:
            if (packetSize >= sizeof(MsgAbortMatch))
            {
                const auto* msg = reinterpret_cast<const MsgAbortMatch*>(packetData);
                cmd.senderSteamId = msg->senderSteamId;
                EnqueueCommand(cmd);
            }
            break;

        case Opcode::MSG_LOBBY_STATE:
            if (packetSize >= sizeof(MsgLobbyState))
            {
                const auto* msg = reinterpret_cast<const MsgLobbyState*>(packetData);
                cmd.mode = msg->mode;
                cmd.arenaId = msg->arenaId;
                cmd.durationSeconds = msg->durationSeconds;
                cmd.scoreLimit = msg->scoreLimit;
                EnqueueCommand(cmd);
            }
            break;

        default:
            SA_LOG_DEBUG("ArenaNetProtocol", "Unhandled packet opcode: 0x%02X", header->opcode);
            break;
        }
    }

    void ArenaNetProtocol::EnqueueCommand(const NetCommand& cmd)
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.push_back(cmd);
    }

    std::optional<NetCommand> ArenaNetProtocol::DequeueCommand()
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (commandQueue_.empty())
            return std::nullopt;

        NetCommand cmd = commandQueue_.front();
        commandQueue_.pop_front();
        return cmd;
    }

} // namespace seamless_arena::native
