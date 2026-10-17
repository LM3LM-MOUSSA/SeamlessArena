// =============================================================================
// EngineBridge.cpp — Strongly-typed libER engine singletons and safety wrappers
// =============================================================================
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Sections 1, 2, 5)
// RESEARCH: rechres/08-game-events-hooks.md        (Section 2)
// =============================================================================

#include "EngineBridge.h"
#include "Logger.h"

#include <Windows.h>

namespace from::CS
{
    void* WorldChrMan::GetLocalPlayer() const noexcept
    {
        // Fail-closed memory safety check: ensure the WorldChrMan instance is readable
        // up to the local player pointer offset.
        constexpr uintptr_t kLocalPlayerOffset = 0x1E508;
        if (!seamless_arena::native::EngineBridge::IsReadable(this, kLocalPlayerOffset + sizeof(void*)))
            return nullptr;

        const char* base = reinterpret_cast<const char*>(this);
        void* const* playerSlot = reinterpret_cast<void* const*>(base + kLocalPlayerOffset);
        if (!playerSlot)
            return nullptr;

        void* player = *playerSlot;
        if (!player || !seamless_arena::native::EngineBridge::IsReadable(player, sizeof(void*)))
            return nullptr;

        return player;
    }
}

namespace seamless_arena::native
{
    bool EngineBridge::IsReadable(const void* ptr, size_t size) noexcept
    {
        if (!ptr || size == 0)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0)
            return false;

        if (mbi.State != MEM_COMMIT)
            return false;

        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
            return false;

        const DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return (mbi.Protect & kReadable) != 0;
    }

    bool EngineBridge::IsSystemInitialized() noexcept
    {
        return from::DLSY::is_system_initialized();
    }

    void* EngineBridge::GetGameWindowHandle() noexcept
    {
        auto windowOpt = from::CS::CSWindow::instance();
        if (!windowOpt.has_reference())
            return nullptr;

        return windowOpt.reference().get_window_handle();
    }

    bool EngineBridge::PlaySystemSound(int soundId) noexcept
    {
        try
        {
            auto soundRef = from::CS::CSSound::play_system_sound('m', soundId);
            return soundRef.get() != nullptr;
        }
        catch (...)
        {
            SA_LOG_WARN("EngineBridge", "Exception caught while attempting to play system sound %d", soundId);
            return false;
        }
    }

    from::CS::WorldChrMan* EngineBridge::GetWorldChrMan() noexcept
    {
        auto chrManOpt = from::CS::WorldChrMan::instance();
        if (!chrManOpt.has_reference())
            return nullptr;

        from::CS::WorldChrMan& ref = chrManOpt.reference();
        if (!IsReadable(&ref, sizeof(void*)))
            return nullptr;

        return &ref;
    }

    void* EngineBridge::GetLocalPlayer() noexcept
    {
        auto* chrMan = GetWorldChrMan();
        if (!chrMan)
            return nullptr;

        return chrMan->GetLocalPlayer();
    }

    bool EngineBridge::IsWorldLoaded() noexcept
    {
        return GetLocalPlayer() != nullptr;
    }

    int32_t EngineBridge::GetLocalPlayerHp() noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return -1;

        // In ER ChrIns: ChrDataModule* is at +0x190
        const char* playerBase = static_cast<const char*>(player);
        constexpr uintptr_t kDataModuleOffset = 0x190;
        if (!IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
            return -1;

        void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
        if (!dataModulePtr || !*dataModulePtr)
            return -1;

        void* dataModule = *dataModulePtr;
        // In ChrDataModule: HP is at +0x138 (int32)
        constexpr uintptr_t kHpOffset = 0x138;
        if (!IsReadable(static_cast<const char*>(dataModule) + kHpOffset, sizeof(int32_t)))
            return -1;

        return *reinterpret_cast<const int32_t*>(static_cast<const char*>(dataModule) + kHpOffset);
    }

    int32_t EngineBridge::GetLocalPlayerMaxHp() noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return -1;

        const char* playerBase = static_cast<const char*>(player);
        constexpr uintptr_t kDataModuleOffset = 0x190;
        if (!IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
            return -1;

        void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
        if (!dataModulePtr || !*dataModulePtr)
            return -1;

        void* dataModule = *dataModulePtr;
        // In ChrDataModule: Max HP is at +0x13C (int32)
        constexpr uintptr_t kMaxHpOffset = 0x13C;
        if (!IsReadable(static_cast<const char*>(dataModule) + kMaxHpOffset, sizeof(int32_t)))
            return -1;

        return *reinterpret_cast<const int32_t*>(static_cast<const char*>(dataModule) + kMaxHpOffset);
    }
}
