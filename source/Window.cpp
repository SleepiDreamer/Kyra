#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Window.h"

#include <assert.h>
#include <iostream>

Window::Window(const int width, const int height, const char* title)
{
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
	}

	m_monitor = glfwGetPrimaryMonitor();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

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
