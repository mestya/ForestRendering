#include "ForestScene.hpp" // 引入类
#include "config.hpp"
#include "core/Bonobo.h"
#include "core/WindowManager.hpp"
#include <chrono>
#include <cstdlib>
#include "vk_main.hpp"

using namespace std::literals::chrono_literals;

int main() {
#if BUILD_VULKAN
    try {
    return RunVkTriangleApp();
  } catch (std::exception const &e) {
    std::cerr << "Vulkan app failed: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Vulkan app failed: unknown error" << std::endl;
  }
  return EXIT_FAILURE;
#else
  Bonobo framework;
  WindowManager &window_manager = framework.GetWindowManager();

  // 创建场景对象
  ForestScene scene(window_manager);

  // 创建窗口
  WindowManager::WindowDatum window_datum{scene.getInputHandler(),
                                          scene.getCamera(),
                                          config::resolution_x,
                                          config::resolution_y,
                                          0,
                                          0,
                                          0,
                                          0};

  GLFWwindow *window = window_manager.CreateGLFWWindow(
      "Forest Scene", window_datum, config::msaa_rate);
  if (window == nullptr) {
    LogError("Failed to get a window");
    return EXIT_FAILURE;
  }

  bonobo::init();

  // 场景初始化
  if (!scene.setup(window)) {
    return EXIT_FAILURE;
  }

  auto last_time = std::chrono::high_resolution_clock::now();

  while (!glfwWindowShouldClose(window)) {
    auto const now_time = std::chrono::high_resolution_clock::now();
    auto const delta_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(now_time -
                                                              last_time);
    last_time = now_time;

    glfwPollEvents();

    if (scene.getInputHandler().GetKeycodeState(GLFW_KEY_ESCAPE) &
        JUST_PRESSED) {
      glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (scene.getInputHandler().GetKeycodeState(GLFW_KEY_M) & JUST_RELEASED) {
      window_manager.ToggleFullscreenStatusForWindow(window);
    }

    scene.update(delta_time_us.count());
    scene.render(window);

    glfwSwapBuffers(window);
  }

  bonobo::deinit();
  return EXIT_SUCCESS;
#endif
}
