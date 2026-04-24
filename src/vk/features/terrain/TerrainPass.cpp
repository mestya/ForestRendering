#include "vk/features/terrain/TerrainPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/scene/Vertex.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <iostream>

namespace vkfw
{
  namespace
  {

    Model createQuad(float const width, float const height,
                     unsigned int const horizontal_split_count,
                     unsigned int const vertical_split_count)
    {
      unsigned int const xSegments = std::max(1u, horizontal_split_count);
      unsigned int const ySegments = std::max(1u, vertical_split_count);

      Model data{};
      data.vertices.reserve((xSegments + 1) * (ySegments + 1));
      data.indices.reserve(xSegments * ySegments * 6);

      // Generate a quad on the XZ plane, centered at the origin.
      glm::vec3 const n = {0.0f, 1.0f, 0.0f};
      for (unsigned int iy = 0; iy <= ySegments; ++iy)
      {
        float const v = static_cast<float>(iy) / static_cast<float>(ySegments);
        float const z = -0.5 * height + v * height;

        for (unsigned int ix = 0; ix <= xSegments; ++ix)
        {
          float const u = static_cast<float>(ix) / static_cast<float>(xSegments);
          float const x = -0.5f * width + u * width;

          data.vertices.push_back(Vertex{
              .pos = {x, 0.0f, z},
              .normal = n,
              .uv = {u, v},
          });
        }
      }

      uint32_t const stride = xSegments + 1;
      for (uint32_t iy = 0; iy < ySegments; ++iy)
      {
        for (uint32_t ix = 0; ix < xSegments; ++ix)
        {
          uint32_t const bl = ix + iy * stride;
          uint32_t const br = (ix + 1) + iy * stride;
          uint32_t const tl = ix + (iy + 1) * stride;
          uint32_t const tr = (ix + 1) + (iy + 1) * stride;

          // CCW in Y-up space; matches `frontFace = eClockwise` after Vulkan NDC Y flip.
          data.indices.push_back(bl);
          data.indices.push_back(br);
          data.indices.push_back(tr);

          data.indices.push_back(tr);
          data.indices.push_back(tl);
          data.indices.push_back(bl);
        }
      }

      return data;
    }

  } // namespace

  vk::raii::DescriptorSetLayout TerrainPass::CreateDescriptorSetLayout(const vk::raii::Device &device)
  {
    vk::DescriptorSetLayoutBinding ub_bind{};
    ub_bind.binding = 0;
    ub_bind.descriptorType = vk::DescriptorType::eUniformBuffer;
    ub_bind.descriptorCount = 1;
    ub_bind.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutBinding smp_bind{};
    smp_bind.binding = 1;
    smp_bind.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    smp_bind.descriptorCount = 1;
    smp_bind.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayoutBinding, 2> binds = {ub_bind, smp_bind};
    vk::DescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.bindingCount = 2;
    dsl_ci.pBindings = binds.data();
    return vk::raii::DescriptorSetLayout{device, dsl_ci};
  }
  vk::raii::DescriptorPool TerrainPass::CreateDescriptorPool(const vk::raii::Device &device, const int image_count)
  {
    uint32_t total_sets = image_count;
    std::array<vk::DescriptorPoolSize, 2> ps{};
    ps[0] = {vk::DescriptorType::eUniformBuffer, total_sets};
    ps[1] = {vk::DescriptorType::eCombinedImageSampler, total_sets};

    vk::DescriptorPoolCreateInfo dp_ci{};
    dp_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    dp_ci.maxSets = total_sets;
    dp_ci.poolSizeCount = 2;
    dp_ci.pPoolSizes = ps.data();
    return vk::raii::DescriptorPool{device, dp_ci};
  }

  void TerrainPass::CreateUniformBuffers(
      const VkContext &ctx,
      const vk::PhysicalDeviceMemoryProperties &mem_props,
      uint32_t image_count)
  {
    // 预留空间，防止 vector 频繁扩容（提高效率）
    ubo_buf_.reserve(image_count);
    ubo_mem_.reserve(image_count);
    ubo_map_.resize(image_count, nullptr); // 假设 ubo_map_ 是 std::vector<void*>

    for (uint32_t i = 0; i < image_count; ++i)
    {
      // 1. 创建 Buffer 对象
      vk::BufferCreateInfo u_ci{};
      u_ci.size = sizeof(CameraUBO);
      u_ci.usage = vk::BufferUsageFlagBits::eUniformBuffer;
      ubo_buf_.emplace_back(ctx.Device(), u_ci);

      // 2. 获取内存需求并分配内存
      auto req = ubo_buf_.back().getMemoryRequirements();
      vk::MemoryAllocateInfo u_mai{};
      u_mai.allocationSize = req.size;

      // 查找支持 CPU 写入 (HostVisible) 且 自动刷新缓存 (HostCoherent) 的内存
      u_mai.memoryTypeIndex = FindMemoryType(
          ctx.PhysicalDevice(),
          req.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

      // 创建 DeviceMemory 并推入容器
      ubo_mem_.emplace_back(ctx.Device(), u_mai);

      // 3. 绑定内存到 Buffer
      ubo_buf_.back().bindMemory(*ubo_mem_.back(), 0);

      // 4. 执行持久化映射 (Persistent Mapping)
      // 映射后指针存入 ubo_map_，之后每帧更新数据只需 memcpy，无需重复 map/unmap
      ubo_map_[i] = ubo_mem_.back().mapMemory(0, sizeof(CameraUBO));
    }
  }

  void TerrainPass::UpdateDescriptorSets(const vk::raii::Device &device, uint32_t image_count)
  {
    for (uint32_t i = 0; i < image_count; ++i)
    {

      for (uint32_t m = 0; m < textures_.size(); ++m)
      {
        vk::DescriptorBufferInfo bi{*ubo_buf_[i], 0, sizeof(CameraUBO)};
        vk::DescriptorImageInfo ii{*common_sampler_, *textures_[m].view, vk::ImageLayout::eShaderReadOnlyOptimal};

        vk::WriteDescriptorSet w_ubo{};
        w_ubo.dstSet = *ds_[i];
        w_ubo.dstBinding = 0;
        w_ubo.descriptorCount = 1;
        w_ubo.descriptorType = vk::DescriptorType::eUniformBuffer;
        w_ubo.pBufferInfo = &bi;

        vk::WriteDescriptorSet w_smp{};
        w_smp.dstSet = *ds_[i];
        w_smp.dstBinding = 1;
        w_smp.descriptorCount = 1;
        w_smp.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w_smp.pImageInfo = &ii;

        device.updateDescriptorSets({w_ubo, w_smp}, {});
      }
    }
  }

  void TerrainPass::CreatePipeline(
      const vk::raii::Device &device,
      const std::string &shader_path,
      vk::Format color_format,
      vk::Format depth_format)
  {
    // 1. 加载并创建着色器模块
    // 注意：shader_module 是局部变量，但在函数结束前管线已经创建完成，所以没问题
    auto const code = ReadFile(shader_path);
    vk::ShaderModuleCreateInfo sm_ci{};
    sm_ci.codeSize = code.size();
    sm_ci.pCode = reinterpret_cast<uint32_t const *>(code.data());
    vk::raii::ShaderModule shader_module{device, sm_ci};

    // 2. 着色器阶段配置
    // 注意：Vert 和 Frag 在同一个 SPV 里（你的代码是这么写的）
    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";

    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

    // 3. 顶点输入 (Vertex Input)
    auto binding = VertexBindingDescriptions();
    auto attrs = VertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = binding.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    // 4. 输入装配 (Input Assembly)
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    // 5. 视口与裁剪 (使用动态状态，所以这里只需指定数量)
    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    // 6. 光栅化 (Rasterization)
    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eNone;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    // 7. 深度测试 (Depth Stencil)
    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 1;
    dss.depthCompareOp = vk::CompareOp::eLess;

    // 8. 多重采样 (Multisample)
    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // 9. 颜色混合 (Color Blend)
    vk::PipelineColorBlendAttachmentState cb_att{};
    cb_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    // 10. 动态状态 (Dynamic State)
    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    // 11. 管线布局 (Pipeline Layout)
    // 假设 dsl_ 已经在外部初始化好了
    vk::PipelineLayoutCreateInfo pl_ci{};
    vk::DescriptorSetLayout raw_dsl = *dsl_;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &raw_dsl;
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    // 12. 动态渲染信息 (Rendering Create Info - KHR_dynamic_rendering)
    vk::PipelineRenderingCreateInfo rendering_ci{};
    rendering_ci.colorAttachmentCount = 1;
    rendering_ci.pColorAttachmentFormats = &color_format;
    if (depth_format != vk::Format::eUndefined)
    {
      rendering_ci.depthAttachmentFormat = depth_format;
    }

    // 13. 创建图形管线
    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci; // 关键：指向动态渲染配置
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp_state;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pDepthStencilState = (depth_format != vk::Format::eUndefined) ? &dss : nullptr;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    gp_ci.layout = *pipeline_layout_;
    gp_ci.renderPass = nullptr; // 使用动态渲染时设为 nullptr

    // 赋值给成员变量 pipeline_
    pipeline_ = vk::raii::Pipeline{device, nullptr, gp_ci};
  }

  void TerrainPass::CreateVertexBuffer(
      const VkContext &ctx,
      const vk::PhysicalDeviceMemoryProperties &mem_props,
      const Model &model)
  {
    vk::BufferCreateInfo bci{};

    bci.size = model.vertices.size() * sizeof(model.vertices[0]);
    bci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    bci.sharingMode = vk::SharingMode::eExclusive;
    vertex_buffer_ = vk::raii::Buffer{ctx.Device(), bci};

    auto req = vertex_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vertex_memory_ = vk::raii::DeviceMemory{ctx.Device(), mai};
    vertex_buffer_.bindMemory(*vertex_memory_, 0);

    void *dst = vertex_memory_.mapMemory(0, bci.size);
    std::memcpy(dst, model.vertices.data(), bci.size);
    vertex_memory_.unmapMemory();
  }

  void TerrainPass::CreateIndexBuffer(
      const VkContext &ctx,
      const vk::PhysicalDeviceMemoryProperties &mem_props,
      const Model &model)
  {
    vk::BufferCreateInfo bci{};

    bci.size = sizeof(model.indices[0]) * model.indices.size();
    bci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
    bci.sharingMode = vk::SharingMode::eExclusive;
    index_buffer_ = vk::raii::Buffer{ctx.Device(), bci};

    auto req = index_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    index_memory_ = vk::raii::DeviceMemory{ctx.Device(), mai};
    index_buffer_.bindMemory(*index_memory_, 0);

    void *dst = index_memory_.mapMemory(0, bci.size);
    std::memcpy(dst, model.indices.data(), bci.size);
    index_memory_.unmapMemory();
  }

  bool TerrainPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();
    // 获取内存属性供 FindMemoryType 使用
    auto const mem_props = ctx.PhysicalDevice().getMemoryProperties();

    {
      terrtain_ = createQuad(500.0f, 500.0f, 1000, 1000);
      CreateVertexBuffer(ctx, mem_props, terrtain_);
      CreateIndexBuffer(ctx, mem_props, terrtain_);
    }

    // 加载纹理 texture_
    LoadTexture(ctx, "res/forested-floor/textures/KiplingerFLOOR.png", 0);
    CreateCommonSampler(device);
    int image_count = swapchain.ImageCount();
    int total_sets = image_count * textures_.size(); // 每个 swapchain image 需要一个描述符集，且每个描述符集包含所有纹理

    {
      dsl_ = CreateDescriptorSetLayout(device);       // define Descriptor Layout
      dp_ = CreateDescriptorPool(device, total_sets); // size of layout; each swapchain image need one set(Descriptor Layout)
      std::vector<vk::DescriptorSetLayout> layouts(total_sets, *dsl_);
      vk::DescriptorSetAllocateInfo ds_ai{};
      ds_ai.descriptorPool = *dp_;
      ds_ai.descriptorSetCount = total_sets;
      ds_ai.pSetLayouts = layouts.data();
      ds_ = device.allocateDescriptorSets(ds_ai);
      CreateUniformBuffers(ctx, mem_props, image_count);
      UpdateDescriptorSets(device, image_count);
    }

    CreatePipeline(device, "res/vk/terrain.spv", swapchain.Format(), targets.shared_depth.format);
    return true;
  }

  void TerrainPass::Destroy(VkContext &ctx)
  {
    // raii handles clean themselves up
    ctx.Device().waitIdle();
    pipeline_ = nullptr;
    pipeline_layout_ = nullptr;
    ds_.clear();
    dp_ = nullptr;
    dsl_ = nullptr;
    index_buffer_ = nullptr;
    index_memory_ = nullptr;
    vertex_buffer_ = nullptr;
    vertex_memory_ = nullptr;

    for (size_t i = 0; i < ubo_map_.size(); ++i)
    {
      if (i < ubo_mem_.size() && ubo_map_[i])
      {
        ubo_mem_[i].unmapMemory();
        ubo_map_[i] = nullptr;
      }
    }
    ubo_map_.clear();
    ubo_mem_.clear();
    ubo_buf_.clear();
    IRenderPass::Destroy(ctx);
  }

  void TerrainPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &)
  {
    // For the minimal triangle, nothing swapchain-sized is owned here.
  }

  void TerrainPass::Record(FrameContext &frame, RenderTargets &targets)
  {

    if (!frame.cmd || !frame.globals)
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    CameraUBO ubo{};
    ubo.view = frame.globals->view;
    ubo.proj = frame.globals->proj;
    ubo.light = frame.globals->light;
    ubo.model = glm::mat4(1.0f);
    ubo.camera_pos = glm::vec4(frame.globals->camera_pos, 1.0f);

    // 确保索引在范围内再拷贝
    if (img < ubo_map_.size() && ubo_map_[img])
    {
      std::memcpy(ubo_map_[img], &ubo, sizeof(ubo));
    }

    vk::ClearValue clear{};
    clear.color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});

    vk::RenderingAttachmentInfo att{};
    att.imageView = frame.swapchain_image_view;
    att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    att.loadOp = vk::AttachmentLoadOp::eLoad;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.clearValue = clear;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

    vk::RenderingAttachmentInfo depth_att{};
    if (targets.shared_depth.Valid())
    {
      depth_att.imageView = targets.shared_depth.view;
      depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
      depth_att.loadOp = vk::AttachmentLoadOp::eLoad;
      depth_att.storeOp = vk::AttachmentStoreOp::eStore;
      depth_att.clearValue = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});
      ri.pDepthAttachment = &depth_att;
    }

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});
    cmd.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint32);
    cmd.bindVertexBuffers(0, *vertex_buffer_, {0});

    if (img < ds_.size())
    {
      cmd.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *pipeline_layout_,
          0,           // 第一个 Set 的索引
          {*ds_[img]}, // 绑定当前帧对应的 Descriptor Set
          {}           // 动态偏移量（如果没有则为空）
      );
    }

    cmd.drawIndexed(static_cast<uint32_t>(terrtain_.indices.size()), 1, 0, 0, 0);
    cmd.endRendering();

    // Transition to present.
    TransitionImage(cmd, frame.swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
  }

  void TerrainPass::updateVertexBuffer()
  {
  }

} // namespace vkfw
