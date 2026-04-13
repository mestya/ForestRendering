#include "vk_main.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/features/gbuffer/GBufferPass.hpp"
#include "vk/features/lighting/LightingPass.hpp"
#include "vk/features/post/PostProcessPass.hpp"
#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/features/ui/ImGuiPass.hpp"
#include "vk/renderer/VkRenderer.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace {
constexpr uint32_t kWidth = 800;
constexpr uint32_t kHeight = 600;
constexpr uint32_t kFramesInFlight = 2;

static void FramebufferResizeCallback(GLFWwindow* window, int, int)
{
  auto* resized = reinterpret_cast<bool*>(glfwGetWindowUserPointer(window));
  if (resized)
    *resized = true;
}
} // namespace

class VkMainApp::Impl {
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
  GLFWwindow* window_ = nullptr;
  bool framebuffer_resized_ = false;

  vkfw::VkContext ctx_{};
  vkfw::VkSwapchain swapchain_{};
  vkfw::VkFrameSync sync_{};
  vkfw::VkRenderer renderer_{};

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
    ci.enable_validation = false;
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
    renderer_.AddPass(std::make_unique<vkfw::ShadowPass>());
    renderer_.AddPass(std::make_unique<vkfw::GBufferPass>());
    renderer_.AddPass(std::make_unique<vkfw::PostProcessPass>());
    renderer_.AddPass(std::make_unique<vkfw::LightingPass>()); // draws the triangle for now
    renderer_.AddPass(std::make_unique<vkfw::ImGuiPass>());

    if (!renderer_.Create(ctx_, swapchain_, sync_))
      throw std::runtime_error("vkfw::VkRenderer::Create failed");
  }

  void RecreateSwapchain()
  {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
      glfwGetFramebufferSize(window_, &w, &h);
      glfwWaitEvents();
    }

    swapchain_.Recreate(ctx_);
    renderer_.OnSwapchainRecreated(ctx_, swapchain_, sync_);
    framebuffer_resized_ = false;
  }

  void MainLoop()
  {
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();

      if (framebuffer_resized_)
        RecreateSwapchain();

      if (!renderer_.DrawFrame(ctx_, swapchain_, sync_))
        RecreateSwapchain();
    }

    ctx_.Device().waitIdle();
  }

  void Cleanup()
  {
    if (ctx_.IsInitialized()) {
      renderer_.Destroy(ctx_);
      swapchain_.Shutdown(ctx_);
      sync_.Shutdown(ctx_);
      ctx_.Shutdown();
    }

    if (window_ != nullptr) {
      glfwDestroyWindow(window_);
      window_ = nullptr;
    }

    glfwTerminate();
  }
};

VkMainApp::VkMainApp() : impl_(new Impl()) {}
VkMainApp::~VkMainApp() = default;
VkMainApp::VkMainApp(VkMainApp&&) noexcept = default;
VkMainApp& VkMainApp::operator=(VkMainApp&&) noexcept = default;

int VkMainApp::Run() { return impl_->Run(); }

int RunVkTriangleApp()
{
  VkMainApp app;
  return app.Run();
}

