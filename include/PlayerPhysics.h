#pragma once
#include <filesystem>
#include <string>

struct GLFWwindow;
struct MCP_Api;
struct MCP_World;
class Camera;

// Drives the camera with Minecraft Java Edition player movement, using the
// original save that a loaded glTF model was exported from.
//
// Everything here is optional at runtime. mcphysics.dll is loaded with
// LoadLibrary and is expected to be missing for anyone who does not have the
// physics project, in which case IsAvailable() stays false and the free camera
// is left alone.
class PlayerPhysics
{
public:
	PlayerPhysics();
	~PlayerPhysics();

	PlayerPhysics(const PlayerPhysics&) = delete;
	PlayerPhysics& operator=(const PlayerPhysics&) = delete;

	// `override` comes from --worlds; empty means search for the worlds folder
	// next to the renderer and physics projects.
	void Initialise(const std::string& worldsRootOverride);

	// Looks for a world folder named after the model file, so "IHOU.glb" pairs
	// with "<worldsRoot>/IHOU/". Quietly does nothing if there is no match.
	void OnModelLoaded(const std::string& modelPath);

	// Hands the camera over to the physics, or gives it back. Returns the new
	// state, which is false if there is nothing to hand over to.
	bool Toggle(Camera& camera);

	// Runs after the camera's orientation has been updated for this frame, so
	// the movement heading matches where the player is actually looking.
	void Update(float deltaTime, GLFWwindow* window, Camera& camera, bool keyboardCaptured);

	// Called for every key press so that a tap between two ticks is not lost.
	// This matters here: at path-tracing frame rates a whole press and release
	// can happen inside one frame.
	void NotifyKeyPress(int glfwKey);

	[[nodiscard]] bool IsAvailable() const { return m_api != nullptr; }
	[[nodiscard]] bool IsWorldLoaded() const { return m_world != nullptr; }
	[[nodiscard]] bool IsEnabled() const { return m_enabled; }
	[[nodiscard]] const std::string& GetWorldName() const { return m_worldName; }

private:
	void CloseWorld();

	void* m_module = nullptr;  // HMODULE, kept opaque to avoid windows.h here
	const MCP_Api* m_api = nullptr;
	MCP_World* m_world = nullptr;

	bool m_enabled = false;
	std::filesystem::path m_worldsRoot;
	std::string m_worldName;

	// Virtual-key bitmap of presses seen since the last update.
	unsigned char m_pendingPresses[32] = {};
};
