# Kyra

### A real-time Path Tracer using DX12 and Slang.

<img width="3072" height="1024" alt="Kyra logo banner image" src="https://github.com/user-attachments/assets/b561547a-1817-45da-b558-9be00c55675a" />

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

**Requirements**
- Nvidia RTX 20 series or higher
- Windows 10 or higher

## Controls
**Movement**
| WASD | A/E | Right-Click | Tab | Shift/Ctrl | Scroll |
| -- | -- | -- | -- | -- | -- |
| Move | Down/Up | Look | Toggle Look | Speed up/down | Change speed |

**Other**
| Alt+Enter | U | H | N |
| -- | -- | -- | -- |
| Toggle Fullscreen | Toggle UI | Toggle HDR | Toggle Denoising |

## Showcase

<img width="49%" height="auto" alt="Screenshot 2026-03-02 235557" src="https://github.com/user-attachments/assets/39d4638c-6cd9-4bac-a979-cbba4f7a8ed7" />
<img width="49%" height="auto" alt="Screenshot 2026-09-06 003242" src="https://github.com/user-attachments/assets/78c1668d-9284-4d22-a543-79e81f5dbd9c" />

<img width="100%" height="auto" alt="Screenshot 2026-09-26 015116" src="https://github.com/user-attachments/assets/efc819ad-6656-4af4-ba39-5e254af69be0" />

<img width="49%" height="auto" alt="Screenshot 2026-07-09 132426" src="https://github.com/user-attachments/assets/2e589e12-82f9-45cc-80f2-8925b11396bf" />
<img width="49%" height="auto" alt="Screenshot 2026-07-01 153147" src="https://github.com/user-attachments/assets/7743c8a6-e5e2-4d53-94a1-dd448005d2bc" />

### Disclaimer

NVIDIA, DLSS, and RTX are trademarks of NVIDIA Corporation. This project is not affiliated with or endorsed by NVIDIA. This software contains source code provided by NVIDIA Corporation.

The Kyra source code is licensed under MIT. This license does not cover third-party components, which remain under their own licenses listed above. See THIRD_PARTY_NOTICES.md for full license texts.
