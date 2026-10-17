// =============================================================================
// dllmain.cpp — Windows dynamic-link library entry point
// =============================================================================
// RESEARCH: rechres/03-me3-native-contract.md (Section 4.1 — Loader Lock Safety)
//
// Keep process attach deliberately minimal. DllMain executes while holding the
// OS loader lock (LdrpLoaderLock). Heavy game memory scanning, MinHook detours,
// and thread spawning inside DllMain risk deadlocks and early access violations.
//
// Per Mod Engine 3 contract, actual initialization is triggered by ME3 calling
// the exported initializer function SeamlessArena_Initialize() in me3_entry.cpp.
// =============================================================================

#include <Windows.h>

static_assert(sizeof(void*) == 8, "SeamlessArena targets 64-bit Elden Ring only.");

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) noexcept
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // Disable DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications for performance
        DisableThreadLibraryCalls(module);
    }

    return TRUE;
}
