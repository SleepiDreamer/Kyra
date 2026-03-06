#pragma once
#include <glm/glm.hpp>

class Light
{
public:
	Light() = default;
	~Light() = default;

	enum class LightType
	{
		Point,
		Directional,
	};

	LightType type = LightType::Point;
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float size = 0.0f;
};

