// =============================================================================
// ArenaNetProtocol.cpp — Wire protocol implementation stubs
// =============================================================================
// RESEARCH: rechres/07-multiplayer-authority-sync.md (ALL sections)
// RESEARCH: rechres/02-seamless-coop-integration.md  (Section 2.2)
// =============================================================================

#include "ArenaNetProtocol.h"
#include "Logger.h"

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // RESEARCH: rechres/02-seamless-coop-integration.md (Section 2.2)
    //
    // PSEUDO:
    //   1. Check if Steam API is available
    //   2. Get ISteamNetworkingMessages interface
    //   3. Register a custom "arena" channel ID to avoid colliding with
    //      Seamless Co-op's own traffic
    //   4. Set up message-received callback
    //
    // TODO(rechres/02, rechres/07):
    //   Investigate whether Seamless Co-op exposes any helper for custom
    //   message routing, or if we must use raw Steam Networking Sockets.
    // -------------------------------------------------------------------------
    bool ArenaNetProtocol::Initialize()
    {
        SA_LOG_INFO("ArenaNetProtocol", "Network protocol initialized (stub).");
        return true;
    }

    void ArenaNetProtocol::Shutdown()
    {
        SA_LOG_INFO("ArenaNetProtocol", "Network protocol shut down.");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 2)
    //
    // PSEUDO:
    //   Validate packetData starts with magic 0x5341
    //   Set sequence number in header
    //   Send via ISteamNetworkingMessages::SendMessageToUser()
    //   Increment nextSequence_
    // -------------------------------------------------------------------------
    void ArenaNetProtocol::SendToHost(const void* packetData, std::size_t packetSize)
    {
        (void)packetData; (void)packetSize;
        // TODO(rechres/07): Implement actual Steam send
    }

    void ArenaNetProtocol::BroadcastToAllPeers(const void* packetData, std::size_t packetSize)
    {
        (void)packetData; (void)packetSize;
        // TODO(rechres/07): Iterate party members, send to each
    }

    void ArenaNetProtocol::SendToPeer(std::uint64_t targetSteamId, const void* packetData, std::size_t packetSize)
    {
        (void)targetSteamId; (void)packetData; (void)packetSize;
        // TODO(rechres/07): Send to specific peer
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 4)
    //
    // PSEUDO:
    //   Lock queueMutex_
    //   If commandQueue_ is empty -> return std::nullopt
    //   Pop front -> return NetCommand
    //
    // TODO(rechres/07 Section 4):
    //   Add sequence number validation to reject stale/duplicate packets.
    //   Add heartbeat timeout detection (5s without MSG_SCORE_SYNC).
    // -------------------------------------------------------------------------
    std::optional<NetCommand> ArenaNetProtocol::DequeueCommand()
    {
        // TODO(rechres/07): Return queued commands
        return std::nullopt;
    }

} // namespace seamless_arena::native
