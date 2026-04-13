#pragma once

#include "Context.hpp"

namespace forest::vk {

class App
{
public:
	void run();

private:
	void init_window();
	void init_vulkan();
	void main_loop();
	void cleanup();

private:
	static void framebuffer_resize_callback(GLFWwindow* window, int, int);

private:
	GLFWwindow* m_window = nullptr;
	Context m_ctx;
};

} // namespace forest::vk

