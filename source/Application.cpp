#include <windows.h>
#include <glfw3.h>

#include "Application.h"
#include "Window.h"
#include "Renderer.h"
#include "Camera.h"

#include <backends/imgui_impl_glfw.h>

Application::Application(const bool debugLayer)
{
	m_window = std::make_unique<Window>(1920, 1080);
	m_renderer = std::make_unique<Renderer>(*m_window, debugLayer);
	m_camera = std::make_shared<Camera>();
	m_camera->SetPosition(glm::vec3(0.0f, 0.0f, 2.0f));
	m_camera->SetDirection(glm::vec3(0.0f, 0.0f, -1.0f));
	m_camera->m_fov = 60.0f;
	m_renderer->SetCamera(m_camera);
	m_renderer->LoadModel("assets/models/Sponza/Sponza.gltf");
	m_renderer->LoadHDRI("assets/environments/christmas_photo_studio_03_2k.hdr");

	auto glfwWindow = m_window->GetGLFWWindow();

	glfwSetWindowUserPointer(glfwWindow, this);
	glfwSetKeyCallback(glfwWindow, KeyCallback);
	glfwSetScrollCallback(glfwWindow, ScrollCallback);
	glfwSetWindowSizeCallback(glfwWindow, WindowSizeCallback);
	glfwSetDropCallback(glfwWindow, DropCallback);

	glfwGetCursorPos(glfwWindow, &m_mouseXPrev, &m_mouseYPrev);
}

Application::~Application()
{
	glfwTerminate();
}

void Application::Run()
{
	auto glfwWindow = m_window->GetGLFWWindow();

	float deltaTime = 0.0f;
	float lastFrameTime = 0.0f;
	while (!glfwWindowShouldClose(glfwWindow))
	{
		float currentFrameTime = static_cast<float>(glfwGetTime());
		deltaTime = std::min(currentFrameTime - lastFrameTime, 0.1f);

		glfwPollEvents();

		Update(deltaTime);

		m_renderer->Render(deltaTime);

		lastFrameTime = currentFrameTime;
	}
}

void Application::Update(const float deltaTime)
{
	auto window = m_window->GetGLFWWindow();

	float cameraSpeed = m_movementSpeed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		cameraSpeed *= 4.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
	{
		cameraSpeed *= 0.25f;
	}

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) // W
	{
		m_camera->Move(deltaTime * m_camera->GetForward() * cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) // S
	{
		m_camera->Move(-deltaTime * m_camera->GetForward() * cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) // A
	{
		m_camera->Move(-deltaTime * m_camera->GetRight() * cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) // D
	{
		m_camera->Move(deltaTime * m_camera->GetRight() * cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) // Q
	{
		m_camera->Move(glm::vec3(0, -deltaTime, 0) * cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) // R
	{
		m_camera->Move(glm::vec3(0, deltaTime, 0) * cameraSpeed);
	}

	double x, y;
	glfwGetCursorPos(window, &x, &y);

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS || m_mouseCaptured)
	{

		const float sensitivity = static_cast<float>(4.0 / m_window->GetWidth());
		glm::vec2 delta = sensitivity * glm::vec2(static_cast<float>(m_mouseXPrev - x), static_cast<float>(m_mouseYPrev - y));
		if (abs(delta.x) > 0.0f || abs(delta.y) > 0.0f)
		{
			m_camera->Rotate(delta);
		}
	}

	m_mouseXPrev = x;
	m_mouseYPrev = y;
}

void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key != GLFW_KEY_TAB)
	{
		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	}

	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		return;
	}

	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if ((key == GLFW_KEY_ENTER && action == GLFW_PRESS) && (mods & GLFW_MOD_ALT))
	{
		app->m_renderer->ToggleFullscreen();
	}
	if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
	{
		app->m_mouseCaptured = !app->m_mouseCaptured;
		if (app->m_mouseCaptured)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported())
			{
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

		glfwGetCursorPos(window, &app->m_mouseXPrev, &app->m_mouseYPrev);
	}
}

void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app)
	{
		app->m_movementSpeed = app->m_movementSpeed * powf(1.1f, static_cast<float>(yoffset));
	}
}

void Application::WindowSizeCallback(GLFWwindow* window, const int width, const int height)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app && app->m_renderer)
	{
		app->m_renderer->Resize(width, height);
	}
}

void Application::DropCallback(GLFWwindow* window, const int count, const char** paths)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app && app->m_renderer)
	{
		for (int i = 0; i < count; ++i)
		{
			std::string extension = std::string(paths[i]).substr(std::string(paths[i]).find_last_of('.'));
			if (extension == ".hdr")
			{
				app->m_renderer->LoadHDRI(paths[i]);
			}
			if (extension == ".gltf" || extension == ".glb")
			{
				app->m_renderer->LoadModel(paths[i]);
			}
		}
	}
}