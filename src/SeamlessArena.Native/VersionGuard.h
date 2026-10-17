#pragma once
// =============================================================================
// VersionGuard.h — Fail-closed game and dependency version checking
// =============================================================================
// RESEARCH: rechres/00-compatibility-target.md  (Sections 2, 3, 4)
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 5)
//
// Before installing ANY hooks, this module verifies:
//   1. The running eldenring.exe matches a known-good PE signature
//   2. ersc.dll (Seamless Co-op) is loaded in process memory
//   3. (Future) libER symbol addresses resolve to valid memory
//
// If ANY check fails, the mod aborts initialization gracefully.
// No hooks are installed. The game continues running unmodified.
// =============================================================================

#include <cstdint>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Result of the full compatibility verification pipeline
    // RESEARCH: rechres/00-compatibility-target.md (Section 3 — matrix)
    // -------------------------------------------------------------------------
    struct VersionCheckResult
    {
        bool eldenRingOk     = false;   // PE timestamp + SizeOfImage matched
        bool seamlessCoopOk  = false;   // ersc.dll found in process
        bool libErOk         = false;   // libER symbols resolved (future)
        bool allPassed       = false;   // Composite: all above are true

        std::uint32_t gameTimestamp   = 0;
        std::uint32_t gameSizeOfImage = 0;
        void*         erscBaseAddr    = nullptr;
    };

    // -------------------------------------------------------------------------
    // Run the full verification pipeline
    // RESEARCH: rechres/00-compatibility-target.md (Section 4 — pseudocode)
    // RESEARCH: rechres/02-seamless-coop-integration.md (Section 3.1 — ersc detect)
    //
    // PSEUDO:
    //   1. GetModuleHandleW(NULL) -> hExe
    //   2. Parse PE DOS header -> NT headers
    //   3. Read TimeDateStamp and SizeOfImage
    //   4. Compare against known-good table (rechres/00 Section 2.1)
    //   5. GetModuleHandleA("ersc.dll") -> check presence
    //   6. (Future) Validate libER singleton pointers (rechres/04 Section 5)
    //   7. Return composite result
    // -------------------------------------------------------------------------
    VersionCheckResult VerifyEnvironment();

    // -------------------------------------------------------------------------
    // Check if a specific game build is in our known-supported table
    // RESEARCH: rechres/00-compatibility-target.md (Section 2.1)
    //
    // PSEUDO:
    //   Iterate over array of { expectedTimestamp, expectedSizeOfImage }
    //   Return true if any entry matches
    // -------------------------------------------------------------------------
    bool IsKnownSupportedBuild(std::uint32_t timestamp, std::uint32_t sizeOfImage);

} // namespace seamless_arena::native
