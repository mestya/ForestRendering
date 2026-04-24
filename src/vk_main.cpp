#include "vk_main.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/features/gbuffer/GBufferPass.hpp"
#include "vk/features/lighting/LightingPass.hpp"
#include "vk/features/post/PostProcessPass.hpp"
#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/features/ui/ImGuiPass.hpp"
#include "vk/features/terrain/TerrainPass.hpp"
#include "vk/features/skybox/SkyboxPass.hpp"
#include "vk/renderer/VkRenderer.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include <imgui_impl_vulkan.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "vk/features/mesh/MeshPass.hpp"

namespace
{
  constexpr uint32_t kWidth = 800;
  constexpr uint32_t kHeight = 600;
  constexpr uint32_t kFramesInFlight = 2;

  struct SimpleCamera
  {
    glm::vec3 pos{0.0f, 10.0f, 50.0f};
    float yaw = -90.0f;
    float pitch = -10.0f;
    float move_speed = 20.0f;
    float mouse_sens = 0.12f;

    glm::vec3 Front() const
    {
      float const yaw_r = glm::radians(yaw);
      float const pitch_r = glm::radians(pitch);
      glm::vec3 f{};
      f.x = std::cos(yaw_r) * std::cos(pitch_r);
      f.y = std::sin(pitch_r);
      f.z = std::sin(yaw_r) * std::cos(pitch_r);
      return glm::normalize(f);
    }

    glm::mat4 View() const
    {
      return glm::lookAt(pos, pos + Front(), glm::vec3{0.0f, 1.0f, 0.0f});
    }

    glm::mat4 Proj(float aspect) const
    {
      glm::mat4 p = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, 0.1f, 200.0f);
      p[1][1] *= -1.0f; // Vulkan 需要翻转 Y
      return p;
    }

    void AddMouseDelta(float dx, float dy)
    {
      yaw += dx * mouse_sens;
      pitch -= dy * mouse_sens;
      pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
  };

  static void FramebufferResizeCallback(GLFWwindow *window, int, int)
  {
    auto *resized = reinterpret_cast<bool *>(glfwGetWindowUserPointer(window));
    if (resized)
      *resized = true;
  }
} // namespace

class VkMainApp::Impl
{
public:
  int Run()
  {
    InitWindow();
    InitVulkanFramework();
    MainLoop();
    Cleanup();
    return 0;
  }

private:
  GLFWwindow *window_ = nullptr;
  bool framebuffer_resized_ = false;

  vkfw::VkContext ctx_{};
  vkfw::VkSwapchain swapchain_{};
  vkfw::VkFrameSync sync_{};
  vkfw::VkRenderer renderer_{};
  glm::vec3 _lightPosition{0.0f, 10.0f, 50.0f};
  vkfw::DebugParam debugParameter_;
  float _sunTime = 0.0f;

  void InitWindow()
  {
    if (glfwInit() == GLFW_FALSE)
      throw std::runtime_error("glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(kWidth, kHeight, "ForestRenderingVk", nullptr, nullptr);
    if (window_ == nullptr)
      throw std::runtime_error("glfwCreateWindow failed");

    glfwSetWindowUserPointer(window_, &framebuffer_resized_);
    glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
  }

  void InitVulkanFramework()
  {
    vkfw::ContextCreateInfo ci{};
    ci.window = window_;
#ifdef NDEBUG
    ci.enable_validation = true;
#else
    ci.enable_validation = true;
#endif
    ctx_.Init(ci);

    sync_.Init(ctx_, kFramesInFlight);

    // Swapchain creates from the window size via VkContext::Window().
    vkfw::SwapchainInfo si{};
    swapchain_.Init(ctx_, si);
    sync_.EnsureRenderFinishedSize(ctx_, swapchain_.ImageCount());

    // Shadow -> GBuffer -> Post -> Lighting -> UI
    std::string modelPath = "res/47-mapletree/MapleTree.obj";
    renderer_.AddPass(std::make_unique<vkfw::SkyboxPass>());
    renderer_.AddPass(std::make_unique<vkfw::MeshPass>(modelPath));
    // renderer_.AddPass(std::make_unique<vkfw::ShadowPass>());
    // renderer_.AddPass(std::make_unique<vkfw::GBufferPass>());
    // renderer_.AddPass(std::make_unique<vkfw::PostProcessPass>());
    // renderer_.AddPass(std::make_unique<vkfw::LightingPass>()); // draws the triangle for now
    renderer_.AddPass(std::make_unique<vkfw::TerrainPass>());
    renderer_.AddPass(std::make_unique<vkfw::ImGuiPass>());
    if (!renderer_.Create(ctx_, swapchain_, sync_, debugParameter_))
      throw std::runtime_error("vkfw::VkRenderer::Create failed");
  }

  void RecreateSwapchain()
  {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0)
    {
      glfwGetFramebufferSize(window_, &w, &h);
      glfwWaitEvents();
    }

    swapchain_.Recreate(ctx_);
    renderer_.OnSwapchainRecreated(ctx_, swapchain_, sync_);
    framebuffer_resized_ = false;
  }

  void MainLoop()
  {
    using clock = std::chrono::high_resolution_clock;
    auto start_time = clock::now();
    auto last_time = start_time;

    SimpleCamera camera{};
    bool mouse_look = false;
    double last_x = 0.0, last_y = 0.0;

    while (!glfwWindowShouldClose(window_))
    {
      glfwPollEvents();

      auto now = clock::now();
      float dt = std::chrono::duration<float>(now - last_time).count();
      float t = std::chrono::duration<float>(now - start_time).count();
      last_time = now;

      // Day/Night Cycle
      if (debugParameter_.animation)
      {
        float deltaTimeUs = (float)(dt / 1000000.0);
        _sunTime += dt * debugParameter_.daySpeed;
      }
      // Simulated solar orbit
      float daySpeed = 0.5f;
      float sunRadius = 100.0f; // Sun distance (For directional light, this value
                                // only affects direction, not attenuation)
      float x_factor = 2.0;
      float y_factor = 4.0;

      if (!debugParameter_.animation)
      {
        _lightPosition.x = sin(_sunTime) * sunRadius;
        _lightPosition.y = cos(_sunTime) * sunRadius;
        _lightPosition.z = -100.0f;
      }
      else
      {
        _lightPosition = glm::vec3(debugParameter_.lightX, debugParameter_.lightY, debugParameter_.lightZ);
      }
      auto light_world_to_clip_matrix = updateLightMatrix(_lightPosition);

      if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);

      // RMB mouse look
      if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
      {
        if (!mouse_look)
        {
          mouse_look = true;
          glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
          glfwGetCursorPos(window_, &last_x, &last_y);
        }
        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        camera.AddMouseDelta(static_cast<float>(x - last_x), static_cast<float>(y - last_y));
        last_x = x;
        last_y = y;
      }
      else if (mouse_look)
      {
        mouse_look = false;
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }

      glm::vec3 f = camera.Front();
      glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3{0.0f, 1.0f, 0.0f}));
      float speed = camera.move_speed * dt;

      if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
        camera.pos += f * speed;
      if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
        camera.pos -= f * speed;
      if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
        camera.pos += r * speed;
      if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
        camera.pos -= r * speed;
      if (glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS)
        camera.pos += glm::vec3{0, 1, 0} * speed;
      if (glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS)
        camera.pos -= glm::vec3{0, 1, 0} * speed;

      vkfw::FrameGlobals globals{};
      light_world_to_clip_matrix;
      globals.light = light_world_to_clip_matrix;
      globals.view = camera.View();
      globals.proj = camera.Proj(float(swapchain_.Extent().width) / float(swapchain_.Extent().height));
      globals.camera_pos = camera.pos;
      globals.light_position = glm::vec3{std::cos(t * 0.2f), std::sin(t * 0.2f), 0.2f};
      globals.time_seconds = t;
      globals.delta_seconds = dt;

      if (!renderer_.DrawFrame(ctx_, swapchain_, sync_, globals))
        RecreateSwapchain();
    }
  }

  glm::mat4 updateLightMatrix(const glm::vec3 light_pos)
  {

    // Defines the size of the area covered by the shadow.
    float boxSize = 150.0f;

    // Near/Far
    // float nearPlane = 1.0f;
    // float farPlane = 3000.0f;
    float nearPlane = 1.0f, farPlane = 1000.5f;
    auto lightProjection =
        glm::ortho(-boxSize, boxSize, -boxSize, boxSize, nearPlane, farPlane);

    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f); // center of forest
    glm::mat4 lightView =
        glm::lookAt(light_pos, target, glm::vec3(0.0, 1.0, 0.2));

    return lightProjection * lightView;
  }

  void Cleanup()
  {
    ctx_.Device().waitIdle();

    if (ctx_.IsInitialized())
    {
      renderer_.Destroy(ctx_);
      swapchain_.Shutdown(ctx_);
      sync_.Shutdown(ctx_);
      ctx_.Shutdown();
    }

    if (window_ != nullptr)
    {
      glfwDestroyWindow(window_);
      window_ = nullptr;
    }

    glfwTerminate();
  }
};

VkMainApp::VkMainApp() : impl_(new Impl()) {}
VkMainApp::~VkMainApp() = default;
VkMainApp::VkMainApp(VkMainApp &&) noexcept = default;
VkMainApp &VkMainApp::operator=(VkMainApp &&) noexcept = default;

int VkMainApp::Run() { return impl_->Run(); }

int RunVkTriangleApp()
{
  VkMainApp app;
  return app.Run();
}
