# Kyra

### A real-time Path Tracer using DX12 and Slang.

<img width="3072" height="1024" alt="KyraBanner" src="https://github.com/user-attachments/assets/b561547a-1817-45da-b558-9be00c55675a" />

## Features
- Fully path traced PBR rendering
- glTF & HDRI loading
- SHaRC radiance caching
- Shader Execution Reordering
- DLSS Ray Reconstruction
- Alias Table power sampling (NEE)
- HDR Output
- Bloom, DoF, AE, AF

## Building
1. Clone the repo `git clone https://github.com/SleepiDreamer/Kyra.git`
2. Build with CMake by running `build.bat`
3. Open the solution in `build/`

## Requirements
- Nvidia RTX GPU
- Windows 10 or higher

## Controls
**Movement**
| WASD | A/E | Right-Click | Tab | Shift/Ctrl | Scroll |
| -- | -- | -- | -- | -- | -- |
| Move | Down/Up | Look | Toggle Look | Speed up/down | Change speed |

**Other**
| U | H | N |
| -- | -- | -- |
| Toggle UI | Toggle HDR | Toggle Denoising |

## Showcase

## Credits

- DirectX 12 Agility SDK: Microsoft DirectX License (binaries), MIT (headers)

NVIDIA, DLSS, and RTX are trademarks of NVIDIA Corporation. This project is not affiliated with or endorsed by NVIDIA. This software contains source code provided by NVIDIA Corporation.

The Kyra source code is licensed under MIT. This license does not cover third-party components, which remain under their own licenses listed above. See THIRD_PARTY_NOTICES.md for full license texts.
