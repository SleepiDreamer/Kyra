# Kyra

### A real-time Path Tracer using DX12 and Slang.

<img width="3072" height="1024" alt="KyraBannerWhite" src="https://github.com/user-attachments/assets/6bf788e5-502a-4de7-bdbb-dee4624f7c92" />

## Features
- Fully path traced PBR rendering
- Shader Execution Reordering
- SHaRC radiance caching
- DLSS Ray Reconstruction
- Alias Table power sampling (NEE w/ MIS)
- HDR Output
- Bloom, DoF, AE, AF
- glTF & HDRI loading
- Shader hot reloading

## Build instructions
1. Clone the repo `git clone https://github.com/SleepiDreamer/Kyra.git`
2. Build with CMake by running `build.bat`
3. Open the solution in `build/`

## Requirements
- Nvidia RTX 20 series or higher
- Windows 10 or higher

## Controls
**Movement**
| WASD | A/E | Right-Click | Tab | Shift/Ctrl | Scroll |
| -- | -- | -- | -- | -- | -- |
| Move | Down/Up | Look | Toggle Look | Speed up/down | Change speed |

**Toggles**
| Alt+Enter | U | H | N |
| -- | -- | -- | -- |
| Fullscreen | UI | HDR | Denoising |

## Showcase
<img width="49.5%" border="2px solid black" alt="Screenshot 2026-03-02 235557-2" src="https://github.com/user-attachments/assets/4b546b58-2922-4054-a777-18a745a0a3ce" />
<img width="49.5%" alt="Screenshot 2026-09-06 003242" src="https://github.com/user-attachments/assets/70e85e40-65ab-4241-aee2-9a16adf8a7da" />

<img width="1536" height="531" alt="Screenshot 2026-09-30 195438" src="https://github.com/user-attachments/assets/520b9adf-c9ac-4d97-a152-79dc6c4b1795" />

<img width="49.5%" alt="Screenshot 2026-03-03 235546" src="https://github.com/user-attachments/assets/737a59f4-f54a-4559-80bc-749c640e39aa" />
<img width="49.5%" alt="Screenshot 2026-07-09 132426" src="https://github.com/user-attachments/assets/2e589e12-82f9-45cc-80f2-8925b11396bf" />

### Disclaimer

NVIDIA, DLSS, and RTX are trademarks of NVIDIA Corporation. This project is not affiliated with or endorsed by NVIDIA. This software contains source code provided by NVIDIA Corporation.

The Kyra source code is licensed under MIT. This license does not cover third-party components, which remain under their own licenses listed above. See THIRD_PARTY_NOTICES.md for full license texts.
