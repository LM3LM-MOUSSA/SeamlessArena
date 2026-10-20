# SeamlessArena Build & Verification Guide

**Last Updated:** 18 October 2026
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

## 3. Recommended Build: CMake

The repository root contains `CMakeLists.txt`. CMake is the preferred build entry point because it builds the Core library, the ME3 native DLL, the rules runner, and the bundled MinHook/libER dependencies from one configuration. It generates Visual Studio projects, so the result still fits the Visual Studio workflow.

Requirements:

- CMake 3.25 or newer; for the `Visual Studio 18 2026` generator, use a CMake release that lists that generator (the VS 2026 bundled CMake 4.3.1 does)
- Visual Studio 2026 C++ workload with the MSVC v145 toolset and Windows SDK
- The MinHook submodule initialized at `deps/minhook`
- The checked-in libER source at `docs/elden api/libER-main`

Configure and build Release x64 from the repository root:

```powershell
cmake -S . -B build-cmake -G "Visual Studio 18 2026" -A x64
cmake --build build-cmake --config Release --parallel
```

For Debug, use `--config Debug`. Outputs are written under `bin/x64/<Configuration>/`, including `SeamlessArena.dll` and `SeamlessArena.Tests.exe`. This matches the paths used by the existing ME3 profile. To generate a Visual Studio solution, open `build-cmake/SeamlessArena.sln` (or open the root `CMakeLists.txt` with Visual Studio's CMake support).

The standalone rules runner can be run separately after the build:

```powershell
& "bin\x64\Release\SeamlessArena.Tests.exe"
```

## 4. Legacy MSBuild Procedure

The existing hand-authored `SeamlessArena.sln` remains available. CMake is the preferred route for new build work; use this route only when maintaining the legacy project files.

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

## 5. Compilation Flags and ABI Consistency

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

## 6. Exported C-ABI Verification

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
