/* Vendored copy of MinecraftPhysics/include/mcphysics.h.
 *
 * It lives here so Kyra builds without the physics repo present: the DLL is
 * loaded at runtime and its absence just means the free camera stays in charge.
 * If the physics repo's copy changes, re-copy this file - a mismatch is caught
 * at runtime by the MCP_ABI_VERSION handshake rather than at link time.
 */
/* mcphysics - Minecraft Java Edition player movement for an external renderer.
 *
 * This is the whole interface between a renderer and the physics DLL. It is
 * plain C, passes no C++ types and lets no exception escape, so it can be
 * loaded with LoadLibrary at runtime and is safe to leave absent: if the DLL
 * is not there, the renderer simply keeps its own free camera.
 *
 * How it is meant to be used
 * --------------------------
 *   1. LoadLibrary("mcphysics.dll"), GetProcAddress("mcp_get_api").
 *   2. api = mcp_get_api(MCP_ABI_VERSION); if it returns NULL the DLL is too
 *      old or too new to talk to, so fall back to the free camera.
 *   3. On loading "World1.glb", look for "<worldsRoot>/World1/". If it has a
 *      region/ subfolder, call api->world_open on it.
 *   4. api->set_eye_position with the current camera position, snapping to the
 *      ground so the camera becomes a valid standing position.
 *   5. Once per frame call api->update with the frame's delta time, the held
 *      key state and the camera's basis vectors. Use the returned
 *      eye_position for the camera; keep owning the orientation yourself.
 *   6. api->world_close when the world is unloaded.
 *
 * Coordinates
 * -----------
 * The renderer's world space is treated as Minecraft's, offset by the contents
 * of "offset.txt" in the world folder (see get_offset). Positions are doubles
 * because that is what the physics works in and it makes set/get round trips
 * exact; direction vectors are floats since normalised directions never need
 * more. Nothing in this header assumes a handedness: the movement heading is
 * derived from the forward vector you pass in, so if Z is mirrored in your
 * export, saying so once in offset.txt is enough.
 */

#ifndef MCPHYSICS_H
#define MCPHYSICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCP_ABI_VERSION 1u

/* The DLL defines MCP_BUILD_DLL. Callers do not need dllimport, because the
 * entry point is meant to be reached through GetProcAddress rather than by
 * linking an import library. */
#if defined(MCP_BUILD_DLL)
#define MCP_API __declspec(dllexport)
#else
#define MCP_API
#endif

/* An opened world. Opaque; several may exist at once. */
typedef struct MCP_World MCP_World;

/* Actions, for bind_key. The DLL owns the keybinds so that the timing-sensitive
 * parts of the game's behaviour (double-tap jump to fly, not dropping a tap
 * that happens between two ticks) stay on this side of the boundary. */
typedef enum {
    MCP_ACTION_FORWARD = 0,
    MCP_ACTION_BACK = 1,
    MCP_ACTION_LEFT = 2,
    MCP_ACTION_RIGHT = 3,
    MCP_ACTION_JUMP = 4,
    MCP_ACTION_SNEAK = 5,
    MCP_ACTION_SPRINT = 6,
    MCP_ACTION_COUNT = 7
} MCP_Action;

/* Defaults: W/A/S/D, space, shift, left control. */

/* MCP_Input.flags */
#define MCP_INPUT_IGNORE_KEYS 0x00000001u /* tick with no input (menu is open) */

/* MCP_State.state_flags */
#define MCP_STATE_ON_GROUND 0x00000001u
#define MCP_STATE_FLYING 0x00000002u
#define MCP_STATE_SPRINTING 0x00000004u
#define MCP_STATE_SNEAKING 0x00000008u
#define MCP_STATE_COLLIDED 0x00000010u /* ran into something horizontally */

typedef struct MCP_Input {
    uint32_t struct_size; /* set to sizeof(MCP_Input) */

    /* Bitmaps of 256 Win32 virtual key codes: bit (vk & 7) of byte (vk >> 3).
     *
     * keys_down is what is held right now. keys_pressed is what went down at
     * least once since the previous update; fill it from WM_KEYDOWN if frames
     * can get long enough to swallow a whole tap, or leave it zeroed and press
     * edges are worked out by diffing keys_down instead. */
    uint8_t keys_down[32];
    uint8_t keys_pressed[32];

    /* The camera's basis in renderer space. Only forward is required; it is
     * projected onto the horizontal plane to get the movement heading, exactly
     * as the game uses yaw alone to walk. right and up are echoed back
     * untouched for convenience. */
    float forward[3];
    float right[3];
    float up[3];

    /* Reserved: the renderer owns the look angles, so these are ignored. They
     * exist so that adding a physics-owned camera mode later does not have to
     * change the size of this struct. */
    float mouse_dx;
    float mouse_dy;

    uint32_t flags;
} MCP_Input;

typedef struct MCP_State {
    uint32_t struct_size; /* set to sizeof(MCP_State) */

    /* Where to put the camera, in renderer space, interpolated between the two
     * surrounding 20 Hz ticks so motion is smooth at any frame rate. */
    double eye_position[3];

    /* The basis passed in, echoed unchanged. */
    float forward[3];
    float right[3];
    float up[3];

    /* Blocks per tick, in renderer space (a direction, so the offset's
     * translation does not apply - only its Z mirror). */
    double velocity[3];
    double eye_height;  /* 1.62 standing, 1.27 sneaking */
    uint32_t state_flags;
} MCP_State;

typedef struct MCP_Api {
    uint32_t abi_version;

    /* Opens a world folder (the one holding level.dat and region/). On failure
     * returns NULL and writes a message into err, if err is non-NULL. */
    MCP_World* (*world_open)(const char* folder, char* err, uint32_t err_size);
    void (*world_close)(MCP_World* world);

    /* Places the player so its eyes are at the given position. Pass
     * renderer_space non-zero for renderer coordinates, zero for raw Minecraft
     * ones. With snap_to_ground non-zero the player is dropped onto the first
     * surface below, which is what turns a free-flying camera position into a
     * valid standing one; returns zero if nothing solid was found. */
    int (*set_eye_position)(MCP_World* world, const double position[3], int renderer_space,
                            int snap_to_ground);

    /* Starts or stops creative flight. Worth switching on when handing over
     * from a free camera, so the transition does not begin with a fall. */
    void (*set_flying)(MCP_World* world, int flying);

    /* The renderer-space offset read from offset.txt, so the renderer can
     * check its export lines up. renderer = minecraft + offset, with Z negated
     * first when flip_z is set. */
    void (*get_offset)(MCP_World* world, double offset[3], int* flip_z);

    /* Advances the simulation by dt_seconds and fills out_state. Returns zero
     * only on a bad argument. Long frames are clamped rather than being
     * allowed to run an unbounded number of catch-up ticks. */
    int (*update)(MCP_World* world, double dt_seconds, const MCP_Input* input,
                  MCP_State* out_state);

    void (*bind_key)(MCP_World* world, MCP_Action action, uint32_t virtual_key);

    /* The last error message for this world, or "" - never NULL. */
    const char* (*last_error)(MCP_World* world);
} MCP_Api;

/* The only export. Ask for the version this header describes; returns NULL if
 * the DLL cannot provide it. */
MCP_API const MCP_Api* mcp_get_api(uint32_t requested_abi_version);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MCPHYSICS_H */
