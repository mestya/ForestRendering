#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vk/scene/Vertex.hpp"

namespace vkfw
{

  struct Model
  {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 center{0.0f};
    float radius = 1.0f;

    static Model LoadFromFile(std::string const &path);
  };

} // namespace vkfw
