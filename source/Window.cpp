#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include "Log.h"
#include "Window.h"

Window::Window(const int width, const int height, const char* title)
{
	if (!glfwInit())
	{
		Log::Error("Failed to initialize GLFW");
	}

	m_monitor = glfwGetPrimaryMonitor();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	GLFWimage images[1];
	images[0].pixels = stbi_load("resources/Kyra.png", &images[0].width, &images[0].height, nullptr, 4);
	glfwSetWindowIcon(m_window, 1, images);
	stbi_image_free(images[0].pixels);

	m_hwnd = glfwGetWin32Window(m_window);
}

int Window::GetWidth() const
{
	int width;
	glfwGetWindowSize(m_window, &width, nullptr);
	return width;
}

int Window::GetHeight() const
{
	int height;
	glfwGetWindowSize(m_window, nullptr, &height);
	return height;
}
