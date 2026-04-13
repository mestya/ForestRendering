#include "App.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace forest::vk {

namespace {
constexpr int k_initial_width = 1280;
constexpr int k_initial_height = 720;
} // namespace

void App::run()
{
	init_window();
	init_vulkan();
	main_loop();
	cleanup();
}

void App::init_window()
{
	if (glfwInit() != GLFW_TRUE) throw std::runtime_error("glfwInit failed");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(k_initial_width, k_initial_height, "ForestSceneVK", nullptr, nullptr);
	if (m_window == nullptr) throw std::runtime_error("glfwCreateWindow failed");

	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);
}

void App::init_vulkan()
{
	m_ctx.init(m_window);
}

void App::main_loop()
{
	while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
		glfwPollEvents();
		m_ctx.draw_frame();
	}
}

void App::cleanup()
{
	m_ctx.cleanup();

	if (m_window) {
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
	glfwTerminate();
}

void App::framebuffer_resize_callback(GLFWwindow* window, int, int)
{
	auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
	if (app != nullptr) {
		app->m_ctx.request_swapchain_recreate();
	}
}

} // namespace forest::vk

