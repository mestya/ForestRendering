# 周冲刺计划（功能切片版，2026-04-13 到 2026-04-18）

## 冲刺目标
- 首周建立“人人一条完整链路”的工作方式。
- A 与 B 各自独立完成最小 Vulkan 渲染闭环（不是只做局部模块）。
- 约束：周日休息，4/13（周一）正式开工。

## 角色说明
- 成员 A：Geometry + Shadow 链路负责人。
- 成员 B：Lighting + Post + UI 链路负责人。

## 每日计划（A/B 并行）

### 2026-04-13（周一）
- 成员 A（Geometry 链）
  - [ ] 建立 A 自己的最小 Vulkan 初始化路径（Instance/Device/Queue）。
  - [ ] 建立 A 的 triangle pipeline 与命令提交框架。
- 成员 B（Lighting 链）
  - [ ] 建立 B 自己的最小 Vulkan 初始化路径（可复用共享内核）。
  - [ ] 建立 B 的 fullscreen triangle/light pass pipeline 框架。
- 当日验收
  - [ ] A/B 两边都能独立跑到可提交命令缓冲。

### 2026-04-14（周二）
- 成员 A
  - [ ] 完成 vertex/index 数据上传与绑定。
  - [ ] 画出 A 的彩色三角形（Geometry 最小输出）。
- 成员 B
  - [ ] 完成 descriptor/set-layout 最小样例（为 lighting 准备）。
  - [ ] 画出 B 的全屏 pass 最小输出。
- 当日验收
  - [ ] A/B 各自都能看到自己的可视化输出。

### 2026-04-15（周三）
- 成员 A
  - [ ] 接入深度 attachment 与基础深度测试。
  - [ ] 预留 shadow map 资源与 pass 框架。
- 成员 B
  - [ ] 接入 GBuffer 输入结构（先用 mock/最小数据）。
  - [ ] 做 lighting pass 的参数更新（push constants/UBO）。
- 当日验收
  - [ ] A/B 各自的 pass 有独立资源与参数更新路径。

### 2026-04-16（周四）
- 成员 A
  - [ ] 完成 resize/recreate 下几何链路稳定性。
  - [ ] 补齐同步（fence/semaphore）并清理关键 validation。
- 成员 B
  - [ ] 完成 resize/recreate 下 lighting 链路稳定性。
  - [ ] 补齐同步并清理关键 validation。
- 当日验收
  - [ ] A/B 的链路均可在 resize 后稳定运行。

### 2026-04-17（周五）
- 成员 A
  - [ ] 输出一条“静态 mesh -> draw”最小路径（可先一个模型）。
  - [ ] 记录几何链路文档（创建资源->录命令->提交->呈现）。
- 成员 B
  - [ ] 输出一条“输入纹理/附件 -> lighting 输出”最小路径。
  - [ ] 记录光照链路文档（descriptor->pipeline->draw->呈现）。
- 当日验收
  - [ ] 两人各有可复现的链路文档与演示结果。

### 2026-04-18（周六）
- 联调日（A+B）
  - [ ] 把 A/B 输出串到同一帧编排（先简单串联）。
  - [ ] 定义下周接口：GBuffer 输出格式、shadow 输入格式、pass 顺序。
  - [ ] 锁定冲突边界（各自只改自己的 feature 文件夹）。
- 当日验收
  - [ ] 主程序可按顺序跑过 A/B 两条功能链。

### 2026-04-19（周日）
- [ ] 休息，不安排开发任务。

## 首周完成定义
- A 独立跑通 Geometry 最小全链路。
- B 独立跑通 Lighting 最小全链路。
- 联调后主程序可串联两条链路，为第二周功能推进做好接口基线。
