# Elden Ring & Dependency Target Capture

**Capture Date:** 17 October 2026  
**Test Environment:** Windows 10 Pro 64-bit (`10.0.19045.0` 22H2)  
**Hardware / System Platform:** x86_64  

---

## 1. Primary Components

| Component | Verified Version / String | Exact File Path (Test Host) | SHA-256 Checksum | Binary Header Metadata |
|---|---|---|---|---|
| **Elden Ring Executable** | App v1.16 / v1.16.1 (`FileVersion 2.7.1.0`) | `F:\SteamLibrary\steamapps\common\ELDEN RING\Game\eldenring.exe` | `B1B6070EEF266D4E46984A47A8181AAF5E791FEC3AAAB46F26D2A2BF97A798CD` | `TimeDateStamp`: `0x69E9C9B9`<br>`SizeOfImage`: `0x05E0A000` |
| **Seamless Co-op Engine** | v1.8.x (`ersc.dll`) | `F:\SteamLibrary\steamapps\common\ELDEN RING\Game\SeamlessCoop\ersc.dll` | `FCD11A1849FD37851BB449BE30F1525CF498F63FD1FA568C4E15030EBE7CB251` | File size: `13,753,872` bytes |
| **Mod Engine 3 (`me3`)** | `me3 0.12.1` | `C:\Users\Rayen_lm3alim\AppData\Local\Programs\garyttierney\me3\bin\me3.exe` | CLI verified via `--version` | Target profile schema: `v1` |
| **Shadow of the Erdtree** | Installed (DLC active) | `F:\SteamLibrary\steamapps\common\ELDEN RING\Game\DLC.bdt` | File size: `15,741,964,105` bytes | DLC map & regulation data present |

---

## 2. Release & Documentation Sources

- **Elden Ring Steam App:** [Steam Store (AppID 1245620)](https://store.steampowered.com/app/1245620/ELDEN_RING/)
- **Bandai Namco Patch Notes (v1.16):** [Bandai Namco Official News](https://en.bandainamcoent.eu/elden-ring/news)
- **Seamless Co-op Official Release Mirror:** [GitHub yuiamoroll/EldenRingSeamlessCoopRelease](https://github.com/yuiamoroll/EldenRingSeamlessCoopRelease)
- **Mod Engine 3 Repository:** [GitHub garyttierney/me3](https://github.com/garyttierney/me3)

---

## 3. PE Header Derivation Procedure

The PE `TimeDateStamp` and `SizeOfImage` values recorded above were extracted directly from the COFF and Optional Headers of `eldenring.exe` on the test machine:

```python
import struct
with open(r'F:\SteamLibrary\steamapps\common\ELDEN RING\Game\eldenring.exe', 'rb') as f:
    f.seek(0x3C)
    pe_off = struct.unpack('<I', f.read(4))[0]
    f.seek(pe_off + 4 + 4)
    timestamp = struct.unpack('<I', f.read(4))[0]
    f.seek(pe_off + 4 + 20 + 56)
    size_of_image = struct.unpack('<I', f.read(4))[0]
    print(f'TimeDateStamp: 0x{timestamp:08X}') # 0x69E9C9B9
    print(f'SizeOfImage:   0x{size_of_image:08X}') # 0x05E0A000
```

These values are configured in [src/SeamlessArena.Native/VersionGuard.cpp](file:///f:/dev/SeamlessArena/src/SeamlessArena.Native/VersionGuard.cpp) to enforce fail-closed verification.
