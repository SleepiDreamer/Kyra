# Kyra
A DX12 path tracing engine

## Walk mode

Minecraft maps loaded as glTF can be walked through with real Minecraft Java Edition
player movement, by pairing the model with the original save it was exported from.
Collision comes from the save's block data rather than the glTF, so invisible `barrier`
blocks stop you and decorative blocks do not.

Press **G** to hand the camera over to the physics, and G again to get the free camera
back. Taking over does not teleport or snap to the ground: the player simply falls from
wherever the camera was.

| Key | In walk mode |
| --- | --- |
| W A S D | Walk |
| Space | Jump, or ascend while flying. Double-tap to toggle creative flight |
| Left Shift | Sneak, or descend while flying |
| Left Ctrl | Sprint |
| Mouse | Look around, exactly as in free-camera mode |

Looking around is unchanged in both modes: the renderer keeps owning the camera
orientation and only the position comes from the physics, so mouse look stays at frame
rate rather than being quantised to Minecraft's 20 Hz tick.

### Setting it up

Walk mode needs two things, both optional at runtime. Without either, Kyra logs one line
and carries on with the free camera.

1. **`mcphysics.dll` next to `Kyra.exe`**, built from the MinecraftPhysics project. It is
   loaded with `LoadLibrary` and reached only through `mcp_get_api`, so a version mismatch
   is caught by the ABI handshake rather than crashing.
2. **A save folder named after the model.** Loading `IHOU.glb` looks for `IHOU/` in the
   worlds folder and needs it to contain a `region/` subfolder:

   ```
   Worlds/
     IHOU/            <- pairs with IHOU.glb
       level.dat
       region/
       offset.txt     <- optional
   ```

The worlds folder is found by searching upwards from the executable and the working
directory for `MinecraftPhysics/Worlds` or `Worlds`, which covers both running from Visual
Studio and running the executable directly. Override it with `--worlds <path>`.

`offset.txt` in a save folder describes how its block coordinates line up with Kyra's
world space (`renderer = minecraft + offset`, with Z negated first if `flip_z` is set).
Without the file the mapping is 1:1.

`external/mcphysics/mcphysics.h` is a vendored copy of the physics project's ABI header so
that Kyra builds whether or not that project is present.
