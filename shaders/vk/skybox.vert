#version 450

layout(location = 0) in vec3 in_pos;

layout(set = 0, binding = 0) uniform SkyboxUBO
{
  mat4 world_to_clip;
  vec4 light_position;
  float time_seconds;
  vec3 _pad;
} ubo;

layout(location = 0) out vec3 localPos;

void main()
{
  localPos = in_pos;
  gl_Position = ubo.world_to_clip * vec4(in_pos, 1.0);
}

