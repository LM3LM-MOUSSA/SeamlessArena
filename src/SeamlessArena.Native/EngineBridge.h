#pragma once
// =============================================================================
// EngineBridge.h — Strongly-typed libER engine singletons and safety wrappers
// =============================================================================
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Sections 1, 2, 5)
// RESEARCH: rechres/08-game-events-hooks.md        (Section 2)
//
// Wraps libER symbols and FromSoftware engine singletons with fail-closed
// memory safety checks. Never dereferences raw pointers without validation.
// =============================================================================

#include <coresystem/window.hpp>
#include <coresystem/sound.hpp>
#include <dantelion2/system.hpp>
#include <fd4/detail/singleton.hpp>

#include <cstdint>
#include <string>

namespace from::CS
{
    // -------------------------------------------------------------------------
    // WorldChrMan — Character & Player Manager singleton
    // Symbol: GLOBAL_WorldChrMan (symbols/singletons.csv: 64380808)
    // -------------------------------------------------------------------------
    class WorldChrMan
    {
    public:
        FD4_SINGLETON_CLASS(WorldChrMan);
        virtual ~WorldChrMan() = default;

        // Returns raw pointer to the local player character instance if valid.
        [[nodiscard]] void* GetLocalPlayer() const noexcept;
    };

    // -------------------------------------------------------------------------
    // CSMenuMan — Menu and UI Manager singleton
    // Symbol: GLOBAL_CSMenuMan (symbols/singletons.csv: 64403376)
    // -------------------------------------------------------------------------
    class CSMenuMan
    {
    public:
        FD4_SINGLETON_CLASS(CSMenuMan);
        virtual ~CSMenuMan() = default;
    };
}

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // High-level engine queries and safety checks
    // -------------------------------------------------------------------------
    class EngineBridge
    {
    public:
        // Returns true if Dantelion2/FD4 core systems are fully initialized.
        [[nodiscard]] static bool IsSystemInitialized() noexcept;

        // Returns true if the main game world and local player are active.
        [[nodiscard]] static bool IsWorldLoaded() noexcept;

        // Win32 window handle from CS::CSWindow.
        [[nodiscard]] static void* GetGameWindowHandle() noexcept;

        // Play an Elden Ring system sound effect (e.g. countdown beep, gong).
        static bool PlaySystemSound(int soundId) noexcept;

        // Pointer to WorldChrMan singleton (or nullptr if uninitialized).
        [[nodiscard]] static from::CS::WorldChrMan* GetWorldChrMan() noexcept;

        // Local player character instance pointer (or nullptr).
        [[nodiscard]] static void* GetLocalPlayer() noexcept;

        // Safely read local player current and max HP.
        [[nodiscard]] static int32_t GetLocalPlayerHp() noexcept;
        [[nodiscard]] static int32_t GetLocalPlayerMaxHp() noexcept;

        // Memory safety guard: verifies memory range is readable.
        [[nodiscard]] static bool IsReadable(const void* ptr, size_t size) noexcept;
    };
}
