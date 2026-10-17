# SeamlessArena Build & Verification Guide

**Last Updated:** 17 October 2026  
**Compiler Platform Toolset:** Visual Studio 2026 (`v145`)  
**Target Architecture:** `x64`  
**C++ Language Standard:** ISO C++20 (`/std:c++20`)  

---

## 1. Toolchain Prerequisites

To build and run SeamlessArena from source, the following components must be installed:
- **Visual Studio 2026** (Community, Professional, Enterprise, or Insiders build with MSVC v145 toolset)
- **Workloads:**
  - *Desktop development with C++*
  - *C++ Core Desktop Features*
  - *MSVC v145 - VS 2026 C++ x64/x86 build tools*
  - *Windows 10/11 SDK (10.0.19041.0 or newer)*
- **Mod Engine 3:** `me3 >= 0.12.1` for game injection and native loading.

---

## 2. Solution Structure

The project solution is organized into three decoupled MSBuild projects:

```text
f:\dev\SeamlessArena\
├── SeamlessArena.sln                 # Top-level solution configuration (Debug/Release x64)
├── SeamlessArena.me3                 # Mod Engine 3 profile definition
├── SeamlessArena.toml                # User-facing runtime configuration
├── src/
│   ├── SeamlessArena.Core/           # Static Library (Pure C++20 match rules, zero external dependencies)
│   ├── SeamlessArena.Native/         # Dynamic Library (ME3 C ABI export, VersionGuard, Coordinator, Hooks)
│   └── SeamlessArena.Tests/          # Console Test Runner (Automated CI validation gate)
└── bin/x64/Release/
    ├── SeamlessArena.Core.lib        # Core static rules library
    ├── SeamlessArena.dll             # Native ME3 extension DLL
    └── SeamlessArena.Tests.exe       # Standalone unit test suite
```

---

## 3. Clean-Clone Build Procedure

Open PowerShell from the repository root:

```powershell
# 1. Locate MSBuild from Visual Studio 2026 installation
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"

# 2. Build Release x64 target
& $msbuild SeamlessArena.sln /p:Configuration=Release /p:Platform=x64

# 3. Verify all 18 automated unit tests pass
& "bin\x64\Release\SeamlessArena.Tests.exe"
```

---

## 4. Compilation Flags and ABI Consistency

To avoid CRT mismatch issues and boundary crashes:

| Setting | `SeamlessArena.Core` | `SeamlessArena.Native` | `SeamlessArena.Tests` |
|---|---|---|---|
| **Configuration Type** | Static Library (`.lib`) | Dynamic Library (`.dll`) | Console Application (`.exe`) |
| **C++ Standard** | `/std:c++20` | `/std:c++20` | `/std:c++20` |
| **Runtime Library** | `/MD` (Release) / `/MDd` (Debug) | `/MD` (Release) / `/MDd` (Debug) | `/MD` (Release) / `/MDd` (Debug) |
| **Warning Level** | `/W4` | `/W4` | `/W4` |
| **Conformance** | `/permissive-` | `/permissive-` | `/permissive-` |
| **Character Set** | Unicode (`/D UNICODE`) | Unicode (`/D UNICODE`) | Unicode (`/D UNICODE`) |

---

## 5. Exported C-ABI Verification

Verify that `SeamlessArena.dll` exports the unmangled C ABI functions required by Mod Engine 3:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe" /exports "bin\x64\Release\SeamlessArena.dll"
```

Expected output:
```text
ordinal hint RVA      name
      1    0 000015C0 SeamlessArena_Finalize = SeamlessArena_Finalize
      2    1 000015A0 SeamlessArena_Initialize = SeamlessArena_Initialize
```
