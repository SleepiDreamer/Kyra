#pragma once
#include <glm/glm.hpp>

enum class LightType : uint32_t
{
	Point = 0, 
	Directional = 1, 
	Triangle = 2,
};

struct PointLightView {
    glm::vec3 position;  uint32_t typeMat;
    glm::vec3 color;     float    radius;
    glm::vec4 _pad[2];
};

struct DirectionalLightView {
    glm::vec3 direction; uint32_t typeMat;
    glm::vec3 color;     float    angularSize;
    glm::vec4 _pad[2];
};

struct TriangleLightView {
    glm::vec3 v0;        uint32_t  typeMat;
    glm::vec3 e1;        float     uv0x;
    glm::vec3 e2;        float     uv0y;
    glm::vec2 uv1;       glm::vec2 uv2;

    LightType type()          const { return static_cast<LightType>(typeMat >> 28); }
    uint32_t  materialIndex() const { return typeMat & 0x00FFFFFFu; }
    glm::vec2 uv0()           const { return { uv0x, uv0y }; }

    void setV0(const glm::vec3 p) { v0 = p; }
    void setE1(const glm::vec3 e) { e1 = e; }
    void setE2(const glm::vec3 e) { e2 = e; }
    void setMaterialIndex(uint32_t i) { typeMat = (typeMat & 0xFF000000u) | (i & 0x00FFFFFFu); }
	void setUV0(const glm::vec2 uv) { uv0x = uv.x; uv0y = uv.y; }
    void setUV1(const glm::vec2 uv) { uv1 = uv; }
    void setUV2(const glm::vec2 uv) { uv2 = uv; }
};

struct Light {
    union {
        glm::vec4             raw[4];
        PointLightView        point;
        DirectionalLightView  directional;
        TriangleLightView     triangle;
    };

    LightType getType() const {
        return static_cast<LightType>(point.typeMat >> 24);
    }
    void setType(LightType t) {
        point.typeMat = (point.typeMat & 0x00FFFFFFu)
            | (static_cast<uint32_t>(t) << 24);
    }
};

static_assert(sizeof(Light) == 64);
static_assert(sizeof(PointLightView) == 64);
static_assert(sizeof(DirectionalLightView) == 64);
static_assert(sizeof(TriangleLightView) == 64);

static_assert(offsetof(PointLightView, typeMat) == 12);
static_assert(offsetof(DirectionalLightView, typeMat) == 12);
static_assert(offsetof(TriangleLightView, typeMat) == 12);