# ForestRendering OpenGL -> Vulkan 迁移总计划（两人功能切片版）

## 目标与边界
- 项目目标：将当前 OpenGL 森林渲染项目迁移到 Vulkan，并确保两位成员都完整实践一条 Vulkan 渲染链路。
- 开始时间：2026-04-13（周一）
- 截止时间：2026-05-20（周三）
- 休息规则：每周日固定休息（不排开发任务）。
- 协作原则：按功能切片分工，不按底层/资源横向拆分。

## 分工方式（功能切片，人人走全链路）
### 成员 A：Geometry + Shadow 功能链
- 负责目标：把“几何与阴影”整条链路做通。
- 完整链路职责：
  - Vulkan 初始化接入（该功能所需的 instance/device/swapchain 适配）
  - 该功能专属 descriptor / pipeline / command recording
  - 该功能的 mesh/instance 数据上传与 draw
  - 呈现结果与调试（validation + RenderDoc）
- 交付功能：地形/树草几何主绘制 + 基础阴影。

### 成员 B：Lighting + Post + UI 功能链
- 负责目标：把“光照与后处理”整条链路做通。
- 完整链路职责：
  - Vulkan 初始化接入（该功能所需资源与同步流程）
  - 该功能专属 descriptor / pipeline / command recording
  - GBuffer/Lighting/SSAO/skybox/ImGui 数据与渲染
  - 呈现结果与调试（validation + RenderDoc）
- 交付功能：Deferred lighting + skybox + UI（SSAO 按进度可降级）。

## 最终合并目标（可合并形态）
- 最终目标架构以 `docs/vulkan-final-architecture.md` 为准（目录、接口、渲染顺序）。
- 共享最小内核（共同维护但改动要小步）：`src/vk/core/*` + `src/vk/renderer/*`。
- Pass 插件接口固定：每个 pass 只暴露 `Create/OnSwapchainRecreated/Record/Destroy`（细节见架构文档）。
- 合并点固定在编排层：最终只在 `VkRenderer::DrawFrame()` 串联各个 pass 的 `Record()`。
- 功能代码分区（各自主责，避免踩文件）：
  - A：`src/vk/features/geometry/*`、`src/vk/features/shadow/*`
  - B：`src/vk/features/gbuffer/*`、`src/vk/features/lighting/*`、`src/vk/features/post/*`、`src/vk/features/ui/*`
- 共享数据通过 `FrameContext`/`RenderTargets` 结构体传递，不在对方 pass 内部直接调用或改状态。

## 每人每天工作量（估算）
- 推荐：每人 `2.5-3.5` 小时/天。
- 冲刺日（周六或联调日前）：每人 `3.5-5` 小时/天。
- 最低可行线：每人 `2` 小时/天（SSAO/体积光/高级粒子降级）。

## 周计划（周日休息）
| 周次 | 日期范围 | A（Geometry+Shadow） | B（Lighting+Post+UI） | 周验收 |
|---|---|---|---|---|
| W1 | 04/13-04/18 | 几何三角形最小链路打通（含 pipeline/vertex/submit/present） | 光照全屏三角最小链路打通（含 pipeline/descriptor/submit/present） | 两人各自能独立画出结果 |
| W2 | 04/20-04/25 | 静态 mesh + 实例数据上传 + 深度通路 | GBuffer 输入准备 + lighting pass 框架 + UI 基础 | 几何与光照链路可串联 |
| W3 | 04/27-05/02 | 阴影 pass（单级）+ 几何主 pass 稳定 | deferred lighting 首版 + skybox 首版 | 场景主画面可见 |
| W4 | 05/04-05/09 | 树草 instancing + 阴影稳定 | UI 参数控制 + 后处理骨架（SSAO blur） | 可交互主画面 |
| W5 | 05/11-05/16 | 阴影质量与性能调整 | SSAO/后处理完成度提升（可降级） | 功能闭环完成 |
| W6 | 05/18-05/20 | 几何/阴影回归与稳定性修复 | 光照/后处理/UI 回归与稳定性修复 | 5/20 可演示版本 |

## 学习目标验收（本计划新增）
- A 验收：能从 0 解释并实现 Geometry+Shadow 全链路。
- B 验收：能从 0 解释并实现 Lighting+Post+UI 全链路。
- 双方都能独立完成：资源创建、pipeline 配置、命令录制、同步与呈现。

## 质量门禁
- 编译门禁：OpenGL 与 Vulkan 目标可构建（按配置）。
- 运行门禁：resize/minimize/restore 不崩溃。
- 验证门禁：无高优先级 Vulkan validation 报错。
- 截止门禁（2026-05-20）：森林主场景可稳定演示。

## 时间紧张时的降级策略
1. 必保留：Geometry 主链、Lighting 主链、UI 基础。
2. 优先降级：SSAO 细节、体积光、复杂粒子。
3. 保证两人学习目标不降级：每人负责的功能链必须完整走通。

