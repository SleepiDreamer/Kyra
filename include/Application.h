#pragma once
#include <glfw3.h>
#include <memory>
#include <string>
#include <vector>

class Window;
class Camera;
class Renderer;

class Application
{
public:
	Application(bool debugLayer, const std::vector<std::string>& inputPaths);
	~Application();

	void Run();
	void Update(float deltaTime);

private:
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void WindowSizeCallback(GLFWwindow* window, int width, int height);
	static void DropCallback(GLFWwindow* window, int count, const char** paths);

	std::unique_ptr<Window> m_window = nullptr;
	std::shared_ptr<Camera> m_camera = nullptr;
	std::unique_ptr<Renderer> m_renderer = nullptr;

	double m_mouseXPrev = 0.0f;
	double m_mouseYPrev = 0.0f;
	float m_movementSpeed = 3.0f;
	bool m_mouseCaptured = false;
};

