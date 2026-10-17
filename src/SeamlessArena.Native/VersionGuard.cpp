// =============================================================================
// VersionGuard.cpp — Fail-closed version verification implementation
// =============================================================================
// RESEARCH: rechres/00-compatibility-target.md  (ALL sections)
// RESEARCH: rechres/02-seamless-coop-integration.md (Section 3.1)
// RESEARCH: rechres/04-liber-abi-memory-safety.md   (Section 5)
// =============================================================================

#include "VersionGuard.h"
#include "Logger.h"

#include <Windows.h>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Known-good Elden Ring builds
    // RESEARCH: rechres/00-compatibility-target.md (Section 2.1)
    //
    // TODO(rechres/00): Fill in the actual TimeDateStamp and SizeOfImage
    //   values from the target Elden Ring v1.16.x build by inspecting the
    //   PE header on your test machine. Run:
    //     dumpbin /headers eldenring.exe
    //   and record TimeDateStamp + SizeOfImage from the OPTIONAL HEADER VALUES.
    // -------------------------------------------------------------------------
    struct KnownBuild
    {
        std::uint32_t timestamp;
        std::uint32_t sizeOfImage;
        const char*   label;
    };

    static constexpr KnownBuild g_knownBuilds[] =
    {
        // Verified on local test machine (Elden Ring v1.16 / v1.16.1 Steam App ID 1245620, FileVersion 2.7.1.0)
        // SHA256: B1B6070EEF266D4E46984A47A8181AAF5E791FEC3AAAB46F26D2A2BF97A798CD
        { 0x69E9C9B9, 0x05E0A000, "Elden Ring v1.16.x (Steam 2.7.1.0)" },
    };

    bool IsKnownSupportedBuild(std::uint32_t timestamp, std::uint32_t sizeOfImage)
    {
        for (const auto& build : g_knownBuilds)
        {
            if (build.timestamp == timestamp && build.sizeOfImage == sizeOfImage)
                return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Full environment verification
    // RESEARCH: rechres/00-compatibility-target.md (Section 4 — pseudocode)
    // RESEARCH: rechres/02-seamless-coop-integration.md (Section 3.1)
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 5)
    // -------------------------------------------------------------------------
    VersionCheckResult VerifyEnvironment()
    {
        VersionCheckResult result{};

        // =====================================================================
        // STEP 1: Verify Elden Ring executable PE header
        // RESEARCH: rechres/00-compatibility-target.md (Section 4)
        //
        // PSEUDO:
        //   hExe = GetModuleHandleW(NULL)
        //   Parse IMAGE_DOS_HEADER -> IMAGE_NT_HEADERS
        //   Extract TimeDateStamp and SizeOfImage
        //   Check against g_knownBuilds[]
        // =====================================================================
        HMODULE hExe = GetModuleHandleW(NULL);
        if (hExe)
        {
            auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hExe);
            if (dos->e_magic == IMAGE_DOS_SIGNATURE)
            {
                auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
                    reinterpret_cast<const char*>(hExe) + dos->e_lfanew);

                if (nt->Signature == IMAGE_NT_SIGNATURE)
                {
                    result.gameTimestamp   = nt->FileHeader.TimeDateStamp;
                    result.gameSizeOfImage = nt->OptionalHeader.SizeOfImage;

                    result.eldenRingOk = IsKnownSupportedBuild(
                        result.gameTimestamp, result.gameSizeOfImage);

                    if (result.eldenRingOk)
                    {
                        SA_LOG_INFO("VersionGuard",
                            "Elden Ring build verified (Timestamp: 0x%08X, Size: 0x%08X)",
                            result.gameTimestamp, result.gameSizeOfImage);
                    }
                    else
                    {
                        SA_LOG_ERROR("VersionGuard",
                            "UNSUPPORTED Elden Ring build! Timestamp: 0x%08X, Size: 0x%08X. "
                            "SeamlessArena will NOT install hooks.",
                            result.gameTimestamp, result.gameSizeOfImage);
                    }
                }
            }
        }

        // =====================================================================
        // STEP 2: Detect Seamless Co-op (ersc.dll) presence
        // RESEARCH: rechres/02-seamless-coop-integration.md (Section 3.1)
        //
        // PSEUDO:
        //   erscHandle = GetModuleHandleA("ersc.dll")
        //   If present -> record base address, mark OK
        //   If absent  -> log warning, arena multiplayer disabled
        // =====================================================================
        HMODULE erscHandle = GetModuleHandleA("ersc.dll");
        result.erscBaseAddr = static_cast<void*>(erscHandle);
        result.seamlessCoopOk = (erscHandle != NULL);

        if (result.seamlessCoopOk)
        {
            SA_LOG_INFO("VersionGuard",
                "Seamless Co-op (ersc.dll) detected at base 0x%p", erscHandle);
        }
        else
        {
            SA_LOG_WARN("VersionGuard",
                "Seamless Co-op (ersc.dll) NOT found. "
                "Multiplayer arena features will be disabled.");
        }

        // =====================================================================
        // STEP 3: (Future) Validate libER engine singleton pointers
        // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 5)
        //
        // PSEUDO:
        //   worldChrMan = CS::WorldChrMan::instance()
        //   if (!worldChrMan || IsBadReadPtr(worldChrMan, sizeof(void*)))
        //       result.libErOk = false
        //       LOG error
        //   else
        //       result.libErOk = true
        //
        // TODO(rechres/04): Implement once libER is integrated into the build.
        //   For now, skip this check and mark as true.
        // =====================================================================
        result.libErOk = true; // PLACEHOLDER — see rechres/04

        // Composite
        result.allPassed = result.eldenRingOk
                        && result.seamlessCoopOk
                        && result.libErOk;

        return result;
    }

} // namespace seamless_arena::native
