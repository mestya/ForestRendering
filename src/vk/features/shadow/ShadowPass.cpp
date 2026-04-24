#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/core/VkContext.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/scene/Vertex.hpp"

namespace vkfw
{
  bool ShadowPass::Create(VkContext &ctx, uint32_t shadow_map_res)
  {
    resolution_ = shadow_map_res;
    auto &device = ctx.Device();

    // 1. 创建阴影图资源 (D32_SFLOAT)
    CreateShadowResources(ctx, resolution_);

    // 2. 创建极简管线 (只有 Vertex Shader, 无 Fragment Shader)
    CreatePipeline(device, vk::Format::eD32Sfloat);

    return true;
  }

  void ShadowPass::Execute(FrameContext &frame,
                           RenderTargets &targets,
                           const std::function<void(vk::raii::CommandBuffer &, vk::PipelineLayout)> &draw_callback)

  {
    auto &cmd = *frame.cmd;

    // 1. 布局转换：Undefined/ShaderRead -> DepthAttachmentOptimal
    TransitionImage(cmd, frame.swapchain_image,
                    vk::ImageLayout::eUndefined,                       // 旧布局
                    vk::ImageLayout::eDepthAttachmentOptimal,          // 新布局：深度附件
                    vk::ImageAspectFlagBits::eDepth,                   // 只影响深度位
                    {},                                                // 之前没操作，Access 为空
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite, // 之后要写深度
                    vk::PipelineStageFlagBits2::eTopOfPipe,            // 尽早开始
                    vk::PipelineStageFlagBits2::eEarlyFragmentTests    // 在深度测试前就得转好
    );

    vk::RenderingAttachmentInfo depth_att{};
    depth_att.imageView = *shadow_view_;
    depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.clearValue.depthStencil = {1.0f, 0};

    vk::RenderingInfo ri{};
    ri.renderArea.extent = vk::Extent2D{resolution_, resolution_};
    ri.layerCount = 1;
    ri.pDepthAttachment = &depth_att;

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

    // 设置视口为阴影图大小
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, (float)resolution_, (float)resolution_, 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {resolution_, resolution_}});

    // 2. 执行外部传入的绘制指令
    // 这个 lambda 会由 App.cpp 提供，内容是：terrain.DrawRaw(cmd) + forest.DrawRaw(cmd)
    draw_callback(cmd, *pipeline_layout_);

    cmd.endRendering();

    TransitionImage(cmd, frame.swapchain_image,
                    vk::ImageLayout::eDepthAttachmentOptimal, // 旧布局：深度附件
                    vk::ImageLayout::eShaderReadOnlyOptimal,  // 新布局：着色器只读
                    vk::ImageAspectFlagBits::eDepth,
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite, // 刚才写完了
                    vk::AccessFlagBits2::eShaderRead,                  // 接下来要读
                    vk::PipelineStageFlagBits2::eLateFragmentTests,    // 必须等深度测试全画完
                    vk::PipelineStageFlagBits2::eFragmentShader        // 在片元着色器采样前转好
    );
  }

  void ShadowPass::CreatePipeline(const vk::raii::Device &device, vk::Format depth_format)
  {
    // 加载 shadow.vert.spv (只有顶点着色器)
    auto const code = ReadFile("res/vk/shadowMap.spv");
    vk::ShaderModuleCreateInfo sm_ci{.codeSize = code.size(), .pCode = (uint32_t *)code.data()};
    vk::raii::ShaderModule shader_module{device, sm_ci};

    vk::PipelineShaderStageCreateInfo stage{};
    stage.stage = vk::ShaderStageFlagBits::eVertex;
    stage.module = *shader_module;
    stage.pName = "main";

    // 顶点输入：和 Terrain 一模一样，这样可以复用同一个 VBO
    auto binding = VertexBindingDescriptions();
    auto attrs = VertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vi{.vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = binding.data(), .vertexAttributeDescriptionCount = (uint32_t)attrs.size(), .pVertexAttributeDescriptions = attrs.data()};

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.depthBiasEnable = VK_TRUE; // 开启深度偏移，防止阴影条纹
    rs.depthBiasConstantFactor = 1.25f;
    rs.depthBiasSlopeFactor = 1.75f;
    rs.lineWidth = 1.0f;

    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = VK_TRUE;
    dss.depthWriteEnable = VK_TRUE;
    dss.depthCompareOp = vk::CompareOp::eLess;

    vk::PipelineLayoutCreateInfo pl_ci{};
    // 这里绑定包含 LightMatrix 的 DescriptorSetLayout
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    vk::PipelineRenderingCreateInfo rendering_ci{.depthAttachmentFormat = depth_format};

    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci;
    gp_ci.stageCount = 1; // 只有顶点阶段
    gp_ci.pStages = &stage;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pDepthStencilState = &dss;
    gp_ci.layout = *pipeline_layout_;

    pipeline_ = vk::raii::Pipeline{device, nullptr, gp_ci};
  }
  void ShadowPass::CreateShadowResources(VkContext &ctx, uint32_t res)
  {
    auto &device = ctx.Device();

    // 1. 创建阴影图 Image
    // 注意：格式必须是深度格式，Usage 必须包含 DepthAttachment 和 Sampled（以便在主 Pass 读取）
    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = vk::Format::eD32Sfloat; // 或者 eD24UnormS8Uint
    ici.extent = vk::Extent3D{res, res, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;

    shadow_image_ = vk::raii::Image{device, ici};

    // 2. 分配并绑定内存
    auto req = shadow_image_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    // 阴影图存在 GPU 中，使用 DeviceLocal 性能最高
    mai.memoryTypeIndex = FindMemoryType(
        ctx.PhysicalDevice(),
        req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    shadow_mem_ = vk::raii::DeviceMemory{device, mai};
    shadow_image_.bindMemory(*shadow_mem_, 0);

    // 3. 创建 ImageView
    // 关键点：subresourceRange.aspectMask 必须是 eDepth
    vk::ImageViewCreateInfo ivci{};
    ivci.image = *shadow_image_;
    ivci.viewType = vk::ImageViewType::e2D;
    ivci.format = vk::Format::eD32Sfloat;
    ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    shadow_view_ = vk::raii::ImageView{device, ivci};

    // 4. 创建专用采样器 (Shadow Sampler)
    // 重点：开启 compareEnable，这样 Shader 采样时会返回 0 或 1 (PCF 基础)
    vk::SamplerCreateInfo sci{};
    sci.magFilter = vk::Filter::eLinear;
    sci.minFilter = vk::Filter::eLinear;
    sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sci.addressModeU = vk::SamplerAddressMode::eClampToEdge; // 边缘外不产生阴影
    sci.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sci.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sci.mipLodBias = 0.0f;
    sci.maxAnisotropy = 1.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 1.0f;
    sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    // 开启硬件深度比较
    sci.compareEnable = VK_TRUE;
    sci.compareOp = vk::CompareOp::eLessOrEqual;

    shadow_sampler_ = vk::raii::Sampler{device, sci};
  }
}