#include <windows.h>

#include "PlayerPhysics.h"

#include "Camera.h"
#include "Log.h"

#include <glfw3.h>

#include <cstring>
#include <vector>

#include "mcphysics.h"

namespace
{
	struct KeyMapping
	{
		int glfwKey;
		unsigned virtualKey;
	};

	constexpr KeyMapping kKeyMappings[] = {
		{ GLFW_KEY_W, 'W' },
		{ GLFW_KEY_A, 'A' },
		{ GLFW_KEY_S, 'S' },
		{ GLFW_KEY_D, 'D' },
		{ GLFW_KEY_SPACE, VK_SPACE },
		{ GLFW_KEY_LEFT_SHIFT, VK_SHIFT },
		{ GLFW_KEY_LEFT_CONTROL, VK_CONTROL },
	};

	void SetKeyBit(unsigned char* bitmap, unsigned virtualKey)
	{
		if (virtualKey > 255)
		{
			return;
		}
		bitmap[virtualKey >> 3] |= static_cast<unsigned char>(1u << (virtualKey & 7u));
	}

	std::filesystem::path FindWorldsRoot()
	{
		std::vector<std::filesystem::path> startPoints;

		wchar_t modulePath[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
		{
			startPoints.emplace_back(std::filesystem::path(modulePath).parent_path());
		}

		std::error_code error;
		std::filesystem::path working = std::filesystem::current_path(error);
		if (!error)
		{
			startPoints.push_back(std::move(working));
		}

		for (const std::filesystem::path& start : startPoints)
		{
			std::filesystem::path directory = start;
			for (int depth = 0; depth < 6; ++depth)
			{
				for (const char* candidate : { "MinecraftPhysics/Worlds", "Worlds" })
				{
					std::filesystem::path root = directory / candidate;
					if (std::filesystem::is_directory(root, error))
					{
						return root;
					}
				}

				if (!directory.has_parent_path() || directory.parent_path() == directory)
				{
					break;
				}
				directory = directory.parent_path();
			}
		}

		return {};
	}
}

PlayerPhysics::PlayerPhysics() = default;

PlayerPhysics::~PlayerPhysics()
{
	CloseWorld();
	if (m_module != nullptr)
	{
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
	}
}

void PlayerPhysics::Initialise(const std::string& worldsRootOverride)
{
	m_module = LoadLibraryA("mcphysics.dll");
	if (m_module == nullptr)
	{
		return;
	}

	using GetApiFunction = const MCP_Api* (*)(uint32_t);
	auto getApi = reinterpret_cast<GetApiFunction>(
		GetProcAddress(static_cast<HMODULE>(m_module), "mcp_get_api"));
	if (getApi == nullptr)
	{
		Log::Error("mcphysics.dll does not export mcp_get_api; walk mode disabled");
		return;
	}

	m_api = getApi(MCP_ABI_VERSION);
	if (m_api == nullptr)
	{
		Log::Error("mcphysics.dll speaks a different ABI than version {}; walk mode disabled",
			static_cast<unsigned>(MCP_ABI_VERSION));
		return;
	}

	if (!worldsRootOverride.empty())
	{
		m_worldsRoot = std::filesystem::path(worldsRootOverride);
		std::error_code error;
		if (!std::filesystem::is_directory(m_worldsRoot, error))
		{
			Log::Warning("--worlds path is not a directory: {}", m_worldsRoot.string());
			m_worldsRoot.clear();
		}
	}
	else
	{
		m_worldsRoot = FindWorldsRoot();
	}

	if (m_worldsRoot.empty())
	{
		Log::Warning("mcphysics.dll loaded but no worlds folder was found; pass --worlds <path>");
		return;
	}

	Log::Success("mcphysics.dll loaded, worlds folder: {}", m_worldsRoot.string());
}

void PlayerPhysics::CloseWorld()
{
	if (m_api != nullptr && m_world != nullptr)
	{
		m_api->world_close(m_world);
	}
	m_world = nullptr;
	m_worldName.clear();
	m_enabled = false;
}

void PlayerPhysics::OnModelLoaded(const std::string& modelPath)
{
	if (m_api == nullptr || m_worldsRoot.empty())
	{
		return;
	}

	const std::string name = std::filesystem::path(modelPath).stem().string();
	const std::filesystem::path folder = m_worldsRoot / name;

	std::error_code error;
	if (!std::filesystem::is_directory(folder / "region", error))
	{
		Log::Info("No Minecraft save for '{}' in {}", name, m_worldsRoot.string());
		return;
	}

	CloseWorld();

	char message[256] = {};
	m_world = m_api->world_open(folder.string().c_str(), message, sizeof(message));
	if (m_world == nullptr)
	{
		Log::Error("Could not open the save for '{}': {}", name, std::string(message));
		return;
	}

	m_worldName = name;

	double offset[3] = {};
	int flipZ = 0;
	m_api->get_offset(m_world, offset, &flipZ);
	Log::Success("Physics world '{}' ready (offset {:+.1f} {:+.1f} {:+.1f}, flip_z {}). Press G to walk.",
		name, offset[0], offset[1], offset[2], flipZ);
}

bool PlayerPhysics::Toggle(Camera& camera)
{
	if (m_api == nullptr)
	{
		Log::Warning("Walk mode needs mcphysics.dll next to the executable");
		return false;
	}
	if (m_world == nullptr)
	{
		Log::Warning("Walk mode needs a Minecraft save matching the loaded model");
		return false;
	}

	m_enabled = !m_enabled;

	if (m_enabled)
	{
		const glm::vec3 position = camera.GetPosition();
		const double eye[3] = { position.x, position.y, position.z };

		m_api->set_flying(m_world, 0);
		m_api->set_eye_position(m_world, eye, /*renderer_space=*/1, /*snap_to_ground=*/0);

		m_unbobbedPosition = position;
		std::memset(m_pendingPresses, 0, sizeof(m_pendingPresses));
		Log::Info("Walk mode on ({}). WASD to move, space to jump, shift to sneak, "
			"ctrl to sprint, double-tap space to fly.", m_worldName);
	}
	else
	{
		camera.SetPosition(m_unbobbedPosition);
		camera.m_fovMultiplier = 1.0f;
		Log::Info("Walk mode off, free camera restored");
	}

	return m_enabled;
}

void PlayerPhysics::NotifyKeyPress(int glfwKey)
{
	if (!m_enabled)
	{
		return;
	}

	for (const KeyMapping& mapping : kKeyMappings)
	{
		if (mapping.glfwKey == glfwKey)
		{
			SetKeyBit(m_pendingPresses, mapping.virtualKey);
			return;
		}
	}
}

void PlayerPhysics::Update(float deltaTime, GLFWwindow* window, Camera& camera,
	bool keyboardCaptured)
{
	if (!m_enabled || m_world == nullptr)
	{
		return;
	}

	MCP_Input input = {};
	input.struct_size = sizeof(input);

	for (const KeyMapping& mapping : kKeyMappings)
	{
		if (glfwGetKey(window, mapping.glfwKey) == GLFW_PRESS)
		{
			SetKeyBit(input.keys_down, mapping.virtualKey);
		}
	}

	std::memcpy(input.keys_pressed, m_pendingPresses, sizeof(input.keys_pressed));
	std::memset(m_pendingPresses, 0, sizeof(m_pendingPresses));

	if (keyboardCaptured)
	{
		input.flags |= MCP_INPUT_IGNORE_KEYS;
	}

	const glm::vec3& forward = camera.GetForward();
	const glm::vec3& right = camera.GetRight();
	const glm::vec3& up = camera.GetUp();
	input.forward[0] = forward.x;
	input.forward[1] = forward.y;
	input.forward[2] = forward.z;
	input.right[0] = right.x;
	input.right[1] = right.y;
	input.right[2] = right.z;
	input.up[0] = up.x;
	input.up[1] = up.y;
	input.up[2] = up.z;

	MCP_State state = {};
	state.struct_size = sizeof(state);

	if (m_api->update(m_world, deltaTime, &input, &state) == 0)
	{
		Log::Error("Physics update failed: {}", std::string(m_api->last_error(m_world)));
		m_enabled = false;
		return;
	}

	m_unbobbedPosition = glm::vec3(static_cast<float>(state.eye_position[0]),
		static_cast<float>(state.eye_position[1]),
		static_cast<float>(state.eye_position[2]));

	const glm::vec3 bobbed = m_unbobbedPosition +
		right * static_cast<float>(state.bob_lateral) +
		up * static_cast<float>(state.bob_vertical);

	camera.SetPosition(bobbed);
	camera.m_fovMultiplier = static_cast<float>(state.fov_multiplier);
}
