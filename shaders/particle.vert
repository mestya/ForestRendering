#version 410 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;
uniform vec3 camera_position;

uniform vec3 light_position;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;
uniform mat4 light_world_to_clip_matrix;

uniform float u_Time;           // Global Time
uniform vec3 u_TreeCrownCenter; // Center point of the tree canopy (particle
                                // emission source)
uniform vec3 u_TreeCrownSize;   // Tree crown size range (e.g., x=5, y=3, z=5)
uniform int isGetDepth;
uniform float wind_strength;
out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
vs_out;
flat out int v_InstanceID;
flat out int v_VertexID;
float random(vec2 st) {
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float waveFun(float time, float A, float f, float p, float k, vec2 D,
              vec3 point) {
  float a = sin((D.x * point.x + (D.y) * point.z) * f + time * p) * 0.5 + 0.5;
  return A * pow(a, k);
}

void main() {
  v_InstanceID = gl_InstanceID;
  v_VertexID = gl_VertexID;
  vs_out.texcoord = texcoord.xy;

  mat4 instanceMatrix =
      mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  vec4 model_pos =
      vertex_model_to_world * instanceMatrix * vec4(in_position * 0.05, 1.0);
  vs_out.normal = (transpose(inverse(vertex_model_to_world * instanceMatrix)) *
                   vec4(normal, 0.0))
                      .rgb;
  float wave1 =
      waveFun(1.0, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), model_pos.xyz);
  float wave2 =
      waveFun(1.0, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), model_pos.xyz);
  float heightOffset = 1.0 * (wave1 + wave2);
  model_pos.y += heightOffset + 4.0;
  float id = float(gl_InstanceID);
  // --- 1. Lifecycle Management ---
  float leafLifeSpan = 25.0;
  // By using time offset,
  // leaves with different IDs are generated at different times.
  float birthTime = id * 0.5;
  float localizedTime = u_Time - birthTime;

  // Calculate the current stage of a leaf's lifecycle (0.0 = newly born, 1.0 =
  // about to disappear) Use mod to implement loop
  float lifePhase = mod(localizedTime, leafLifeSpan) / leafLifeSpan;

  // --- 2. Determine birth location  ---
  vec3 spawnOffset;
  spawnOffset.x = (random(vec2(id, 1.0)) - 0.5) * u_TreeCrownSize.x;
  spawnOffset.y = (random(vec2(id, 2.0)) - 0.5) * u_TreeCrownSize.y;
  spawnOffset.z = (random(vec2(id, 3.0)) - 0.5) * u_TreeCrownSize.z;
  vec3 startPosition = u_TreeCrownCenter + spawnOffset;
  //	vec3 finalVertexPos = vec3(100.0);
  // --- 3. Calculate the falling animation ---
  vec3 currentPos = startPosition;
  // Y-axis: Uniform falling speed
  float dropSpeed =
      pow(0.8 * random(vec2(id, 4.0)), 2.0) * u_Time + 1.0; // Random speed
  dropSpeed = wind_strength * dropSpeed;
  currentPos.y -= lifePhase * leafLifeSpan * dropSpeed;
  float new_height = model_pos.y + currentPos.y;
  vec3 CalPos = model_pos.xyz + currentPos;
  // float boundary = 40;
  if (new_height > model_pos.y) {
    // XZ axis: Drifting with the wind (sine wave)
    float wobbleFreq = 0.4 * wind_strength;
    float wobbleAmp = 10.5;
    currentPos.x += sin(u_Time * wobbleFreq + id) * wobbleAmp;
    currentPos.z += cos(u_Time * wobbleFreq + id * 1.5) * wobbleAmp;
  } else {
    new_height = model_pos.y;
  }
  // --- 4.  Hide after landing ---
  float scale = 1.0;
  float boundary = 40.0;
  float fadeMargin = 10.0;
  // A. Time fades out: The last 20% of life shrinks.
  if (lifePhase > 0.8) {
    scale *= (1.0 - (lifePhase - 0.8) / 0.2);
  }
  // B. Boundary fade-out: Shrinks as it approaches the boundary,
  float distZ = abs(CalPos.z);
  float distX = abs(CalPos.x);
  if (distZ > (boundary - fadeMargin)) {
    scale *= clamp((boundary - distZ) / fadeMargin, 0.0, 1.0);
  }
  if (distX > (boundary - fadeMargin)) {
    scale *= clamp((boundary - distX) / fadeMargin, 0.0, 1.0);
  }

  vec3 finalVertexPos = model_pos.xyz + currentPos;

  vs_out.fV =
      camera_position - vec3(finalVertexPos.x, new_height, finalVertexPos.z);
  vs_out.fL = light_position;

  // The logic for scaling and disappearing after landing is as follows:
  // final position = center point + (shape offset * scaling).
  vec3 shapeOffset =
      (vertex_model_to_world * instanceMatrix * vec4(in_position * 0.05, 0.0))
          .xyz;
  vec3 worldCenterPos =
      vec3(finalVertexPos.x, new_height, finalVertexPos.z) - shapeOffset;
  vec3 finalWorldPos = worldCenterPos + shapeOffset * scale;
  if (isGetDepth == 1) {
    gl_Position = light_world_to_clip_matrix * vec4(finalWorldPos, 1.0);
  } else {
    gl_Position = vertex_view_to_projection * vertex_world_to_view *
                  vec4(finalWorldPos, 1.0);
  }

  //  gl_Position =
  //		vertex_view_to_projection * vertex_world_to_view *
  //		vec4(vec3(finalVertexPos.x, new_height, finalVertexPos.z) *
  // scale, 1.0);
  if (scale == 0.0) {
    gl_Position = vec4(0.0, 0.0, -1000.0, 1.0); // fix Vertex stretching
  }
}
