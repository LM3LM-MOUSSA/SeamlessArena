# 00 — Compatibility Target Freeze

## 1. Overview and Rationale

SeamlessArena operates as an in-process native extension inside Elden Ring, loaded alongside Seamless Co-op through Mod Engine 3 (`me3`). Because Elden Ring and Seamless Co-op frequently update memory offsets, engine data structures, and function signatures with each patch, this project strictly adheres to a **fail-closed compatibility policy**:

> **Rule:** If the running executable, Seamless Co-op DLL, or ME3 environment does not match a verified compatibility profile, the mod MUST cleanly abort initialization, log an actionable error message, and allow the game to continue running normally without installing any hooks or patches.

---

## 2. Target Component Versions

### 2.1. Elden Ring Runtime
- **Target Platform:** Steam (x86_64 Windows)
- **App ID:** `1245620`
- **Primary Game Version:** Elden Ring v1.16 / v1.16.1 (Shadow of the Erdtree DLC enabled or disabled)
- **Executable:** `eldenring.exe`
- **Verification Strategy:**
  1. Inspect PE Header `TimeDateStamp` and `SizeOfImage`.
  2. Read version resource (`VS_FIXEDFILEINFO` via `GetFileVersionInfoW`).
  3. Scan a unique engine anchor pattern (e.g., `WorldChrMan` singleton pointer signature) during early bootstrap.

### 2.2. Seamless Co-op (`ersc.dll`)
- **Developer:** LukeYui / Yui Amoroll
- **Repository Mirror:** `https://github.com/LukeYui/EldenRingSeamlessCoopRelease` & `https://github.com/yuiamoroll/EldenRingSeamlessCoopRelease`
- **Target Release:** Seamless Co-op v1.8.x+ (latest stable release supporting Elden Ring 1.16+)
- **Binary Name:** `ersc.dll`
- **Dependencies / Config:** `ersc_settings.ini`, `SeamlessCoop/` data directory.
- **Save Suffix:** `.co2` (independent of vanilla `.sl2` saves).

### 2.3. Mod Engine 3 (`me3`)
- **Developer:** Gary Tierney
- **Repository:** `https://github.com/garyttierney/me3`
- **Minimum Release:** `me3 v0.13.0+`
- **Loader Feature Requirement:** Support for `load_early = true` and `modengine_ext_init` handoff protocol to initialize `ersc.dll` before game main loop.

---

## 3. Compatibility Matrix

| Elden Ring Version | Seamless Co-op | ME3 Version | Support Status | Action on Mismatch |
|---|---|---|---|---|
| **v1.16.x** | **v1.8.x** | **>= 0.13.0** | **Tier 1 (Target)** | Normal load and full arena features enabled |
| v1.14.x - v1.15.x | v1.7.x | >= 0.12.0 | Legacy Tier | Log warning, abort hooks (fail closed) |
| Unknown / New Patch | Any | Any | Unsupported | Log version mismatch details, abort hooks |
| Any | Missing `ersc.dll` | Any | Unsupported | Log missing dependency, disable arena |
| Vanilla Online (EAC) | N/A | N/A | Blocked | Never load under Easy Anti-Cheat |

---

## 4. Game Version Detection Implementation

The native initialization stage (`SeamlessArena_Initialize` in `SeamlessArena.Native`) executes the version guard before instantiating hooks:

```cpp
// Pseudocode for CompatibilityGuard::VerifyEnvironment()
struct VersionTarget {
    DWORD expectedTimestamp;
    DWORD expectedSizeOfImage;
    const char* semanticVersion;
};

bool VerifyEldenRingVersion() {
    HMODULE hExe = GetModuleHandleW(NULL);
    if (!hExe) return false;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hExe;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uintptr_t)hExe + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    DWORD timestamp = nt->FileHeader.TimeDateStamp;
    DWORD sizeOfImage = nt->OptionalHeader.SizeOfImage;

    // Validate against known hashes/signatures
    if (!IsKnownSupportedBuild(timestamp, sizeOfImage)) {
        Logger::Error("Unsupported Elden Ring build detected! TimeDateStamp: 0x%08X, SizeOfImage: 0x%08X",
                      timestamp, sizeOfImage);
        return false;
    }

    Logger::Info("Elden Ring version verified successfully (Timestamp: 0x%08X).", timestamp);
    return true;
}
```

---

## 5. Verification Checklist for Developers

- [x] Document target game build IDs and binary identification methods.
- [x] Document target Seamless Co-op binary expectations and save format isolation.
- [x] Formulate a fail-closed policy preventing bad memory writes on game updates.
- [ ] Record exact Steam BuildID and sha256 checksum of `eldenring.exe` and `ersc.dll` from the test machine.
