#pragma once
#include <glm/glm.hpp>

#include <filesystem>
#include <string>

struct GLFWwindow;
struct MCP_Api;
struct MCP_World;
class Camera;

class PlayerPhysics
{
public:
	PlayerPhysics();
	~PlayerPhysics();

	PlayerPhysics(const PlayerPhysics&) = delete;
	PlayerPhysics& operator=(const PlayerPhysics&) = delete;

	void Initialise(const std::string& worldsRootOverride);
	void OnModelLoaded(const std::string& modelPath);
	bool Toggle(Camera& camera);
	void Update(float deltaTime, GLFWwindow* window, Camera& camera, bool keyboardCaptured);
	void NotifyKeyPress(int glfwKey);

	[[nodiscard]] bool IsAvailable() const { return m_api != nullptr; }
	[[nodiscard]] bool IsWorldLoaded() const { return m_world != nullptr; }
	[[nodiscard]] bool IsEnabled() const { return m_enabled; }
	[[nodiscard]] const std::string& GetWorldName() const { return m_worldName; }

private:
	void CloseWorld();

	void* m_module = nullptr;
	const MCP_Api* m_api = nullptr;
	MCP_World* m_world = nullptr;

	bool m_enabled = false;
	std::filesystem::path m_worldsRoot;
	std::string m_worldName;

	unsigned char m_pendingPresses[32] = {};

	glm::vec3 m_unbobbedPosition{ 0.0f, 0.0f, 0.0f };
};
