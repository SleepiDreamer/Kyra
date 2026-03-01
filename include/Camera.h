#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	Camera() = default;
	~Camera() = default;

	void SetPosition(const glm::vec3& position) { m_position = position; }
	void SetDirection(const glm::vec3& forward);
	void Move(const glm::vec3& delta);
	void Rotate(const glm::vec2& delta);

	[[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
	[[nodiscard]] const glm::vec3& GetForward() const { return m_forward; }
	[[nodiscard]] const glm::vec3& GetRight() const { return m_right; }
	[[nodiscard]] const glm::vec3& GetUp() const { return m_up; }
	[[nodiscard]] glm::mat4 GetViewMatrix() const;

	float m_fov = 60.0f;
	float m_aperture = 1.0f;
	float m_focusDistance = 1.0f;
private:
	glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 m_forward{ 0.0f, 0.0f, -1.0f };
	glm::vec3 m_right{ 1.0f, 0.0f, 0.0f };
	glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };
};