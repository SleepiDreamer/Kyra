#pragma once
#include <glm/glm.hpp>

enum class LightType : uint32_t
{
	Point = 0, 
	Directional = 1, 
	Triangle = 2,
};

struct PointLightView {
    glm::vec3 position;  uint32_t type;
    glm::vec3 color;     float    radius;
    glm::vec4 _pad[2];
};

struct DirectionalLightView {
    glm::vec3 direction; uint32_t type;
    glm::vec3 color;     float    angularSize;
    glm::vec4 _pad[2];
};

struct TriangleLightView {
    glm::vec3 v0;        uint32_t type;
    glm::vec3 e1;        float    area;
    glm::vec3 e2;        float    _unused;
    glm::vec3 normal;    uint32_t materialIdx;
};

struct Light {
    union {
        glm::vec4             raw[4];
        PointLightView        point;
        DirectionalLightView  directional;
        TriangleLightView     triangle;
    };
};

static_assert(sizeof(Light) == 64);
static_assert(sizeof(PointLightView) == 64);
static_assert(sizeof(DirectionalLightView) == 64);
static_assert(sizeof(TriangleLightView) == 64);

static_assert(offsetof(PointLightView, type) == 12);
static_assert(offsetof(DirectionalLightView, type) == 12);
static_assert(offsetof(TriangleLightView, type) == 12);