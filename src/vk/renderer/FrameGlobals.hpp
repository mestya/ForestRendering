#pragma once
#include <glm/glm.hpp>

namespace vkfw
{

// 每帧全局数据：由应用层更新，渲染层只读取（不持有、不修改）。
struct FrameGlobals {
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  glm::mat4 light{1.0f};
  glm::vec3 camera_pos{0.0f, 0.0f, 0.0f};
  glm::vec3 light_position{0.0f, 1.0f, 0.0f};
    float time_seconds = 0.0f;
    float delta_seconds = 0.0f;
  };

} // namespace vkfw
