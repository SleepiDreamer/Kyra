#ifndef MCPHYSICS_H
#define MCPHYSICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCP_ABI_VERSION 2u

#if defined(MCP_BUILD_DLL)
#define MCP_API __declspec(dllexport)
#else
#define MCP_API
#endif

typedef struct MCP_World MCP_World;

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

#define MCP_INPUT_IGNORE_KEYS 0x00000001u

#define MCP_STATE_ON_GROUND 0x00000001u
#define MCP_STATE_FLYING 0x00000002u
#define MCP_STATE_SPRINTING 0x00000004u
#define MCP_STATE_SNEAKING 0x00000008u
#define MCP_STATE_COLLIDED 0x00000010u

typedef struct MCP_Input {
    uint32_t struct_size;

    uint8_t keys_down[32];
    uint8_t keys_pressed[32];

    float forward[3];
    float right[3];
    float up[3];

    float mouse_dx;
    float mouse_dy;

    uint32_t flags;
} MCP_Input;

typedef struct MCP_State {
    uint32_t struct_size;

    double eye_position[3];

    float forward[3];
    float right[3];
    float up[3];

    double velocity[3];

    double eye_height;

    double fov_multiplier;

    double bob_lateral;
    double bob_vertical;

    uint32_t state_flags;
} MCP_State;

typedef struct MCP_Api {
    uint32_t abi_version;

    MCP_World* (*world_open)(const char* folder, char* err, uint32_t err_size);
    void (*world_close)(MCP_World* world);

    int (*set_eye_position)(MCP_World* world, const double position[3], int renderer_space,
                            int snap_to_ground);

    void (*set_flying)(MCP_World* world, int flying);

    void (*get_offset)(MCP_World* world, double offset[3], int* flip_z);

    int (*update)(MCP_World* world, double dt_seconds, const MCP_Input* input,
                  MCP_State* out_state);

    void (*bind_key)(MCP_World* world, MCP_Action action, uint32_t virtual_key);

    const char* (*last_error)(MCP_World* world);
} MCP_Api;

MCP_API const MCP_Api* mcp_get_api(uint32_t requested_abi_version);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MCPHYSICS_H 
