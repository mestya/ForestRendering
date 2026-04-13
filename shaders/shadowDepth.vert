#version 410

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 binormal;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;
layout(location = 11) in float instanceWindsSpeed;

uniform int lables;
uniform int isGbufferDepth;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;
uniform mat4 vertex_model_to_world;

uniform mat4 light_world_to_clip_matrix;

// 接收时间与风力
uniform float elapsed_time_s;
uniform float wind_strength;

out VS_OUT { vec2 texcoord; }
vs_out;

out vec3 FragPos;
out vec3 Normal;

float random(float seed) { return fract(sin(seed) * 43758.5453123); }

// Rotation matrix auxiliary function
mat3 angleAxis(float angle, vec3 axis) {
  axis = normalize(axis);
  float s = sin(angle);
  float c = cos(angle);
  float oc = 1.0 - c;
  return mat3(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,
              oc * axis.z * axis.x + axis.y * s,
              oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c,
              oc * axis.y * axis.z - axis.x * s,
              oc * axis.z * axis.x - axis.y * s,
              oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c);
}

float waveFun(float time, float A, float f, float p, float k, vec2 D,
              vec3 point) {
  float a = sin((D.x * point.x + (D.y) * point.z) * f + time * p) * 0.5 + 0.5;
  return A * pow(a, k);
}

float derivativeMain(float time, float A, float f, float p, float k, vec2 D,
                     vec3 point) {
  float wave = waveFun(time, A, f, p, max(0, k - 1.0), D, point);
  return 0.5 * k * f * wave *
         cos((D.x * point.x + (D.y) * point.z) * f + time * p);
}

void applyWind(inout vec3 localPos, inout vec3 localNormal,
               inout vec3 localTangent, int labelType, float windPower,
               float time, float seed) {

  if (windPower <= 0.0)
    return;

  switch (labelType) {
  case 1: // Leaves
  {
    // 1. Macroscopic swaying (simulating tree branch swaying)
    float swayAngle = sin(time * 1.5 + seed) * 0.1 * windPower;
    vec3 swayAxis = vec3(0.0, 0.0, 1.0); // arond z
    mat3 rotSway = angleAxis(swayAngle, swayAxis);

    // 2. Microscopic trembling (simulating the random trembling of leaves)
    float flutterPhase = dot(localPos, vec3(10.0, 20.0, 30.0));
    float flutterAngle = sin(time * 15.0 + flutterPhase) * 0.2 * windPower;

    // random axis jitter
    vec3 flutterAxis = normalize(
        vec3(random(seed), random(seed + 123.0), random(seed + 456.0)));
    mat3 rotFlutter = angleAxis(flutterAngle, flutterAxis);

    // 3. Combine and apply
    mat3 totalRot = rotSway * rotFlutter;

    // Rotate local coordinates and normal/tangent lines
    localPos = totalRot * localPos;
    localNormal = totalRot * localNormal;
    localTangent = totalRot * localTangent;
    break;
  }

  case 3: // Grass
  {
    float wave = sin(time * 3.0 + seed + localPos.x * 2.0);
    float bend = pow(max(0.0, localPos.y), 1.5);

    float displacement = wave * bend * 1.8 * windPower;
    localPos.x += displacement;
    localPos.z += displacement * 0.5;
    break;
  }

  default:
    break;
  }
}

void main() {
  int v_InstanceID = gl_InstanceID;
  int v_VertexID = gl_VertexID;
  vs_out.texcoord = texcoord.xy;
  mat4 model_to_world;
  // vs_out.normal_model_to_world = transpose(inverse(model_to_world));
  vec3 world_pos;
  if (lables == 0) {
    model_to_world = vertex_model_to_world;
  } else {
    mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2,
                               instanceMatrix3, instanceMatrix4);
    model_to_world = vertex_model_to_world * instanceMatrix;
  }
  float scale = 1.0;
  if (lables == 3) {
    scale = 0.008;
  }

  vec3 localPos = vertex * scale;
  vec3 localNormal = normal;
  vec3 localTangent = tangent;

  float instanceSpeed = instanceWindsSpeed > 0.0 ? instanceWindsSpeed : 1.0;
  float finalWind = wind_strength * instanceSpeed;

  applyWind(localPos, localNormal, localTangent, lables, finalWind,
            elapsed_time_s, float(v_InstanceID));

  world_pos = vec3(model_to_world * vec4(localPos, 1.0));

  float time = 1.0;
  float wave1 = waveFun(time, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), vertex);
  float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), vertex);

  float heightOffset = 1.0 * (wave1 + wave2);
  world_pos.y += heightOffset;
  if (lables == 3) {
    world_pos.y = world_pos.y + 5.0;
  }
  if (lables == 1 || lables == 2) {
    world_pos.y += 3.0;
  }

  if (isGbufferDepth == 1) {
    FragPos = (vertex_world_to_view * vec4(world_pos, 1.0)).xyz;
    Normal = vec4(transpose(inverse(vertex_world_to_view * model_to_world)) *
                  vec4(normal, 0.0))
                 .xyz;
    gl_Position =
        vertex_view_to_projection * vertex_world_to_view * vec4(world_pos, 1.0);

  } else {
    gl_Position = light_world_to_clip_matrix * vec4(world_pos, 1.0);
  }
}
