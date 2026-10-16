#include <Windows.h>

static_assert(sizeof(void*) == 8, "SeamlessArena targets 64-bit Elden Ring only.");

// Keep process attach deliberately small. Game hooks and runtime startup belong
// in a later bootstrap path, after the loader lock has been released.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) noexcept
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
    }

    return TRUE;
}
