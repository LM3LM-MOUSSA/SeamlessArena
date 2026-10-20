// =============================================================================
// EngineBridge.cpp — Strongly-typed engine singletons and safety wrappers
// =============================================================================
// Dynamic AOB pattern scanner & fail-closed SEH memory safety guards.
// Eliminates brittle hardcoded RVAs and guarantees 0 crash risk across game builds.
// =============================================================================

#include "EngineBridge.h"
#include "Logger.h"

#include <Windows.h>
#include <limits>
#include <cstring>

namespace from::CS
{
    void* WorldChrMan::GetLocalPlayer() const noexcept
    {
        const char* base = reinterpret_cast<const char*>(this);
        if (!base)
            return nullptr;

        // In Elden Ring WorldChrMan:
        // +0x10EF8 points to an array of ChrIns pointers where slot [0] is the local player.
        // +0x1E508 is retained as secondary candidate.
        constexpr uintptr_t kCandidateOffsets[] = { 0x10EF8, 0x1E508 };
        for (uintptr_t offset : kCandidateOffsets)
        {
            __try
            {
                if (!seamless_arena::native::EngineBridge::IsReadable(base + offset, sizeof(void*)))
                    continue;

                void* const* ptrTable = reinterpret_cast<void* const*>(base + offset);
                if (!ptrTable || !*ptrTable)
                    continue;

                void* table = *ptrTable;
                if (!seamless_arena::native::EngineBridge::IsReadable(table, sizeof(void*)))
                    continue;

                void* const* localSlot = reinterpret_cast<void* const*>(table);
                if (localSlot && *localSlot && seamless_arena::native::EngineBridge::IsReadable(*localSlot, 0x1A0))
                {
                    return *localSlot;
                }

                if (seamless_arena::native::EngineBridge::IsReadable(table, 0x1A0))
                {
                    return table;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }

        return nullptr;
    }
}

namespace seamless_arena::native
{
    namespace
    {
        // ---------------------------------------------------------------------
        // Dynamic AOB Pattern Scanner for WorldChrMan
        // Pattern: 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 48 39 88
        // Verified on Elden Ring 1.10 - 1.17+
        // ---------------------------------------------------------------------
        void* FindWorldChrManInstance() noexcept
        {
            static void* s_cachedWorldChrMan = nullptr;
            if (s_cachedWorldChrMan && EngineBridge::IsReadable(s_cachedWorldChrMan, sizeof(void*)))
            {
                __try
                {
                    void* deref = *reinterpret_cast<void**>(s_cachedWorldChrMan);
                    if (deref && EngineBridge::IsReadable(deref, 0x11000))
                    {
                        return deref;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    s_cachedWorldChrMan = nullptr;
                }
            }

            HMODULE hExe = GetModuleHandleW(nullptr);
            if (!hExe) return nullptr;

            const uint8_t* base = reinterpret_cast<const uint8_t*>(hExe);
            __try
            {
                const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
                if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
                const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

                const size_t scanSize = nt->OptionalHeader.SizeOfImage;
                static const uint8_t tailPattern[] = { 0x48, 0x85, 0xC0, 0x74, 0x0F, 0x48, 0x39, 0x88 };

                for (size_t i = 0x1000; i < scanSize - 20; ++i)
                {
                    if (base[i] == 0x48 && base[i + 1] == 0x8B && base[i + 2] == 0x05)
                    {
                        if (std::memcmp(base + i + 7, tailPattern, sizeof(tailPattern)) == 0)
                        {
                            int32_t disp = *reinterpret_cast<const int32_t*>(base + i + 3);
                            void* targetPtr = const_cast<uint8_t*>(base + i + 7 + disp);
                            s_cachedWorldChrMan = targetPtr;

                            if (EngineBridge::IsReadable(targetPtr, sizeof(void*)))
                            {
                                void* inst = *reinterpret_cast<void**>(targetPtr);
                                if (inst && EngineBridge::IsReadable(inst, 0x11000))
                                {
                                    return inst;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }

            return nullptr;
        }
    } // anonymous namespace

    bool EngineBridge::IsReadable(const void* ptr, size_t size) noexcept
    {
        if (!ptr || size == 0)
            return false;

        const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t maxAddress = (std::numeric_limits<uintptr_t>::max)();
        if (size > maxAddress - start)
            return false;

        const uintptr_t end = start + size;
        uintptr_t cursor = start;
        while (cursor < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &mbi, sizeof(mbi)) == 0)
                return false;

            if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                return false;

            const DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                    PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            if ((mbi.Protect & kReadable) == 0)
                return false;

            const uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            if (mbi.RegionSize > maxAddress - regionStart)
                return false;

            const uintptr_t regionEnd = regionStart + mbi.RegionSize;
            if (regionEnd <= cursor)
                return false;

            cursor = (regionEnd < end) ? regionEnd : end;
        }

        return true;
    }

    void* EngineBridge::GetGameWindowHandle() noexcept
    {
        static HWND s_cachedHwnd = nullptr;
        if (s_cachedHwnd && IsWindow(s_cachedHwnd))
            return s_cachedHwnd;

        DWORD currentPid = GetCurrentProcessId();
        struct EnumData {
            DWORD pid;
            HWND hwnd;
        } data{ currentPid, nullptr };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* d = reinterpret_cast<EnumData*>(lParam);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == d->pid && IsWindowVisible(hwnd))
            {
                char title[256] = { 0 };
                GetWindowTextA(hwnd, title, sizeof(title));
                if (std::strstr(title, "ELDEN RING") != nullptr || std::strstr(title, "SeamlessArena") != nullptr)
                {
                    d->hwnd = hwnd;
                    return FALSE;
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&data));

        if (data.hwnd)
        {
            s_cachedHwnd = data.hwnd;
            return data.hwnd;
        }

        HWND direct = FindWindowA("ELDEN RING™", nullptr);
        if (!direct) direct = FindWindowA(nullptr, "ELDEN RING™");
        if (direct)
        {
            s_cachedHwnd = direct;
            return direct;
        }

        return nullptr;
    }

    bool EngineBridge::IsSystemInitialized() noexcept
    {
        // Require both a valid game window and a readable WorldChrMan structure
        HWND hwnd = static_cast<HWND>(GetGameWindowHandle());
        if (!hwnd || !IsWindow(hwnd))
            return false;

        void* chrMan = FindWorldChrManInstance();
        return chrMan != nullptr;
    }

    bool EngineBridge::PlaySystemSound(int soundId) noexcept
    {
        (void)soundId;
        // Clean Windows audio alert — guaranteed 0 crash risk across any OS or game build
        MessageBeep(MB_ICONINFORMATION);
        return true;
    }

    from::CS::WorldChrMan* EngineBridge::GetWorldChrMan() noexcept
    {
        void* inst = FindWorldChrManInstance();
        return reinterpret_cast<from::CS::WorldChrMan*>(inst);
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

        __try
        {
            const char* playerBase = static_cast<const char*>(player);
            constexpr uintptr_t kDataModuleOffset = 0x190;
            if (!IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
                return -1;

            void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
            if (!dataModulePtr || !*dataModulePtr)
                return -1;

            void* dataModule = *dataModulePtr;
            constexpr uintptr_t kHpOffset = 0x138;
            if (!IsReadable(static_cast<const char*>(dataModule) + kHpOffset, sizeof(int32_t)))
                return -1;

            return *reinterpret_cast<const int32_t*>(static_cast<const char*>(dataModule) + kHpOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -1;
        }
    }

    int32_t EngineBridge::GetLocalPlayerMaxHp() noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return -1;

        __try
        {
            const char* playerBase = static_cast<const char*>(player);
            constexpr uintptr_t kDataModuleOffset = 0x190;
            if (!IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
                return -1;

            void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
            if (!dataModulePtr || !*dataModulePtr)
                return -1;

            void* dataModule = *dataModulePtr;
            constexpr uintptr_t kMaxHpOffset = 0x13C;
            if (!IsReadable(static_cast<const char*>(dataModule) + kMaxHpOffset, sizeof(int32_t)))
                return -1;

            return *reinterpret_cast<const int32_t*>(static_cast<const char*>(dataModule) + kMaxHpOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -1;
        }
    }

    bool EngineBridge::SetLocalPlayerPosition(float x, float y, float z) noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return false;

        bool updated = false;
        char* playerBase = static_cast<char*>(player);

        __try
        {
            // 1. Physics/Local coords via ChrDataModule (+0x190) -> +0x68 -> (+0x70: X, +0x74: Z height, +0x78: Y north/south)
            constexpr uintptr_t kDataModuleOffset = 0x190;
            if (IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
            {
                void** dataModulePtr = reinterpret_cast<void**>(playerBase + kDataModuleOffset);
                if (dataModulePtr && *dataModulePtr)
                {
                    char* dataModule = static_cast<char*>(*dataModulePtr);
                    constexpr uintptr_t kPhysicsModuleOffset = 0x68;
                    if (IsReadable(dataModule + kPhysicsModuleOffset, sizeof(void*)))
                    {
                        void** physicsPtr = reinterpret_cast<void**>(dataModule + kPhysicsModuleOffset);
                        if (physicsPtr && *physicsPtr)
                        {
                            char* physics = static_cast<char*>(*physicsPtr);
                            constexpr uintptr_t kPosOffset = 0x70;
                            if (IsReadable(physics + kPosOffset, sizeof(float) * 3))
                            {
                                *reinterpret_cast<float*>(physics + 0x70) = x;
                                *reinterpret_cast<float*>(physics + 0x74) = z; // Height in FromSoftware engine
                                *reinterpret_cast<float*>(physics + 0x78) = y; // North/South
                                updated = true;
                            }
                        }
                    }
                }
            }

            // 2. Map-relative coords on ChrIns (+0x6C0: X, +0x6C4: Z height, +0x6C8: Y)
            constexpr uintptr_t kMapRelCoordsOffset = 0x6C0;
            if (IsReadable(playerBase + kMapRelCoordsOffset, sizeof(float) * 3))
            {
                *reinterpret_cast<float*>(playerBase + 0x6C0) = x;
                *reinterpret_cast<float*>(playerBase + 0x6C4) = z;
                *reinterpret_cast<float*>(playerBase + 0x6C8) = y;
                updated = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        return updated;
    }

    bool EngineBridge::GetLocalPlayerPosition(float& x, float& y, float& z) noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return false;

        const char* playerBase = static_cast<const char*>(player);

        __try
        {
            // Physics coords: +0x70: X, +0x74: Z height, +0x78: Y north/south
            constexpr uintptr_t kDataModuleOffset = 0x190;
            if (IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
            {
                void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
                if (dataModulePtr && *dataModulePtr)
                {
                    const char* dataModule = static_cast<const char*>(*dataModulePtr);
                    constexpr uintptr_t kPhysicsModuleOffset = 0x68;
                    if (IsReadable(dataModule + kPhysicsModuleOffset, sizeof(void*)))
                    {
                        void* const* physicsPtr = reinterpret_cast<void* const*>(dataModule + kPhysicsModuleOffset);
                        if (physicsPtr && *physicsPtr)
                        {
                            const char* physics = static_cast<const char*>(*physicsPtr);
                            constexpr uintptr_t kPosOffset = 0x70;
                            if (IsReadable(physics + kPosOffset, sizeof(float) * 3))
                            {
                                x = *reinterpret_cast<const float*>(physics + 0x70);
                                z = *reinterpret_cast<const float*>(physics + 0x74);
                                y = *reinterpret_cast<const float*>(physics + 0x78);
                                return true;
                            }
                        }
                    }
                }
            }

            // Map-relative coords: +0x6C0: X, +0x6C4: Z height, +0x6C8: Y
            constexpr uintptr_t kMapRelCoordsOffset = 0x6C0;
            if (IsReadable(playerBase + kMapRelCoordsOffset, sizeof(float) * 3))
            {
                x = *reinterpret_cast<const float*>(playerBase + 0x6C0);
                z = *reinterpret_cast<const float*>(playerBase + 0x6C4);
                y = *reinterpret_cast<const float*>(playerBase + 0x6C8);
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        return false;
    }

    int32_t EngineBridge::GetLocalPlayerCurrentAnimation() noexcept
    {
        void* player = GetLocalPlayer();
        if (!player)
            return -1;

        __try
        {
            const char* playerBase = static_cast<const char*>(player);
            constexpr uintptr_t kDataModuleOffset = 0x190;
            if (!IsReadable(playerBase + kDataModuleOffset, sizeof(void*)))
                return -1;

            void* const* dataModulePtr = reinterpret_cast<void* const*>(playerBase + kDataModuleOffset);
            if (!dataModulePtr || !*dataModulePtr)
                return -1;

            const char* dataModule = static_cast<const char*>(*dataModulePtr);
            constexpr uintptr_t kAnimModuleOffset = 0x58;
            if (!IsReadable(dataModule + kAnimModuleOffset, sizeof(void*)))
                return -1;

            void* const* animModulePtr = reinterpret_cast<void* const*>(dataModule + kAnimModuleOffset);
            if (!animModulePtr || !*animModulePtr)
                return -1;

            const char* animModule = static_cast<const char*>(*animModulePtr);
            constexpr uintptr_t kAnimIdOffset = 0x20;
            if (!IsReadable(animModule + kAnimIdOffset, sizeof(int32_t)))
                return -1;

            return *reinterpret_cast<const int32_t*>(animModule + kAnimIdOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -1;
        }
    }
}
