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

    static vk::VertexInputBindingDescription getBindingDescription()
    {
      return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3> AttrDescs()
    {
      return {
          vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
          vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
          vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)),
      };
    }

    static std::vector<char> ReadFile(std::string const &filename)
    {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);
      if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + filename);

      std::vector<char> buffer(static_cast<size_t>(file.tellg()));
      file.seekg(0, std::ios::beg);
      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      file.close();
      return buffer;
    }

    static uint32_t FindMemoryType(vk::PhysicalDeviceMemoryProperties const &mem_props,
                                   uint32_t type_bits,
                                   vk::MemoryPropertyFlags required)
    {
      for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
      {
        if ((type_bits & (1u << i)) == 0u)
          continue;
        if ((mem_props.memoryTypes[i].propertyFlags & required) == required)
          return i;
      }
      throw std::runtime_error("Failed to find suitable memory type");
    }

    static void TransitionImage(vk::raii::CommandBuffer &cmd,
                                vk::Image image,
                                vk::ImageLayout old_layout,
                                vk::ImageLayout new_layout,
                                vk::AccessFlags2 src_access,
                                vk::AccessFlags2 dst_access,
                                vk::PipelineStageFlags2 src_stage,
                                vk::PipelineStageFlags2 dst_stage)
    {
      vk::ImageMemoryBarrier2 barrier{};
      barrier.srcStageMask = src_stage;
      barrier.srcAccessMask = src_access;
      barrier.dstStageMask = dst_stage;
      barrier.dstAccessMask = dst_access;
      barrier.oldLayout = old_layout;
      barrier.newLayout = new_layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = image;
      barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;

      vk::DependencyInfo dep{};
      dep.imageMemoryBarrierCount = 1;
      dep.pImageMemoryBarriers = &barrier;
      cmd.pipelineBarrier2(dep);
    }

  } // namespace

  bool TerrainPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();

    // Shaders: reuse the existing demo SPIR-V that contains vertMain/fragMain.
    auto const code = ReadFile("res/09_shader_base.spv");
    vk::ShaderModuleCreateInfo sm_ci{};
    sm_ci.codeSize = code.size();
    sm_ci.pCode = reinterpret_cast<uint32_t const *>(code.data());
    vk::raii::ShaderModule shader_module{device, sm_ci};
    terrtain_ = createQuad(500.0f, 500.0f, 1000, 1000);
    LoadTexture(ctx, "res/forested-floor/textures/KiplingerFLOOR.png", 0);

    CreateCommonSampler(device);

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
      dsl_ = vk::raii::DescriptorSetLayout{device, dsl_ci};

      uint32_t image_count = swapchain.ImageCount();
      uint32_t total_sets = image_count;
      std::array<vk::DescriptorPoolSize, 2> ps{};
      ps[0] = {vk::DescriptorType::eUniformBuffer, total_sets};
      ps[1] = {vk::DescriptorType::eCombinedImageSampler, total_sets};

      vk::DescriptorPoolCreateInfo dp_ci{};
      dp_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
      dp_ci.maxSets = total_sets;
      dp_ci.poolSizeCount = 2;
      dp_ci.pPoolSizes = ps.data();
      dp_ = vk::raii::DescriptorPool{device, dp_ci};

      std::vector<vk::DescriptorSetLayout> layouts(total_sets, *dsl_);
      vk::DescriptorSetAllocateInfo ds_ai{};
      ds_ai.descriptorPool = *dp_;
      ds_ai.descriptorSetCount = total_sets;
      ds_ai.pSetLayouts = layouts.data();
      ds_ = device.allocateDescriptorSets(ds_ai);

      ubo_buf_.reserve(image_count);
      ubo_mem_.reserve(image_count);
      ubo_map_.resize(image_count, nullptr);

      // 获取内存属性供 FindMemoryType 使用
      auto const mem_props = ctx.PhysicalDevice().getMemoryProperties();

      for (uint32_t i = 0; i < image_count; ++i)
      {
        // 1. 创建 Uniform Buffer
        vk::BufferCreateInfo u_ci{};
        u_ci.size = sizeof(CameraUBO);
        u_ci.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        ubo_buf_.push_back(vk::raii::Buffer{device, u_ci});

        // 2. 分配并绑定内存 (HostVisible 确保 CPU 可写)
        auto req = ubo_buf_.back().getMemoryRequirements();
        vk::MemoryAllocateInfo u_mai{};
        u_mai.allocationSize = req.size;
        u_mai.memoryTypeIndex = FindMemoryType(mem_props, req.memoryTypeBits,
                                               vk::MemoryPropertyFlagBits::eHostVisible |
                                                   vk::MemoryPropertyFlagBits::eHostCoherent);

        ubo_mem_.push_back(vk::raii::DeviceMemory{device, u_mai});
        ubo_buf_.back().bindMemory(*ubo_mem_.back(), 0);

        // 3. 映射内存，拿到 CPU 端指针 ubo_map_[i]
        ubo_map_[i] = ubo_mem_.back().mapMemory(0, sizeof(CameraUBO));

        // 4. 更新描述符集 (只更新 Binding 0)
        vk::DescriptorBufferInfo bi{*ubo_buf_[i], 0, sizeof(CameraUBO)};

        vk::DescriptorImageInfo ii{*common_sampler_, *textures_[0].view, vk::ImageLayout::eShaderReadOnlyOptimal};

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

        // 执行更新
        device.updateDescriptorSets({w_ubo, w_smp}, {});
      }
    }

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

    auto binding = vkfw::getBindingDescription();
    auto attrs = vkfw::AttrDescs();
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eNone;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 1;
    dss.depthCompareOp = vk::CompareOp::eLess;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState cb_att{};
    cb_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    vk::PipelineLayoutCreateInfo pl_ci{};
    vk::DescriptorSetLayout raw_dsl = *dsl_;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &raw_dsl;
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    vk::Format const color_format = swapchain.Format();

    vk::PipelineRenderingCreateInfo rendering_ci{};
    rendering_ci.colorAttachmentCount = 1;
    rendering_ci.pColorAttachmentFormats = &color_format;
    if (targets.shared_depth.Valid())
      rendering_ci.depthAttachmentFormat = targets.shared_depth.format;

    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci;
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp_state;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pDepthStencilState = targets.shared_depth.Valid() ? &dss : nullptr;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    gp_ci.layout = *pipeline_layout_;
    gp_ci.renderPass = nullptr;

    pipeline_ = vk::raii::Pipeline{device, nullptr, gp_ci};

    auto const mem_props = ctx.PhysicalDevice().getMemoryProperties();

    {
      vk::BufferCreateInfo bci{};

      bci.size = terrtain_.vertices.size() * sizeof(terrtain_.vertices[0]);
      bci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
      bci.sharingMode = vk::SharingMode::eExclusive;
      vertex_buffer_ = vk::raii::Buffer{device, bci};

      auto req = vertex_buffer_.getMemoryRequirements();
      vk::MemoryAllocateInfo mai{};
      mai.allocationSize = req.size;
      mai.memoryTypeIndex = FindMemoryType(mem_props, req.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      vertex_memory_ = vk::raii::DeviceMemory{device, mai};
      vertex_buffer_.bindMemory(*vertex_memory_, 0);

      void *dst = vertex_memory_.mapMemory(0, bci.size);
      std::memcpy(dst, terrtain_.vertices.data(), bci.size);
      vertex_memory_.unmapMemory();
    }

    {
      vk::BufferCreateInfo bci{};

      bci.size = sizeof(terrtain_.indices[0]) * terrtain_.indices.size();
      bci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
      bci.sharingMode = vk::SharingMode::eExclusive;
      index_buffer_ = vk::raii::Buffer{device, bci};

      auto req = index_buffer_.getMemoryRequirements();
      vk::MemoryAllocateInfo mai{};
      mai.allocationSize = req.size;
      mai.memoryTypeIndex = FindMemoryType(mem_props, req.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      index_memory_ = vk::raii::DeviceMemory{device, mai};
      index_buffer_.bindMemory(*index_memory_, 0);

      void *dst = index_memory_.mapMemory(0, bci.size);
      std::memcpy(dst, terrtain_.indices.data(), bci.size);
      index_memory_.unmapMemory();
    }

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

    // float t = 0.0f;
    // cmd.pushConstants<float>(*pipeline_layout_, vk::ShaderStageFlagBits::eFragment, 0, t);
    cmd.drawIndexed(static_cast<uint32_t>(terrtain_.indices.size()), 1, 0, 0, 0);
    cmd.endRendering();

    // Transition to present.
    TransitionImage(cmd,
                    frame.swapchain_image,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::AccessFlagBits2::eColorAttachmentWrite,
                    {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits2::eBottomOfPipe);
  }

  void TerrainPass::updateVertexBuffer()
  {
  }

} // namespace vkfw
