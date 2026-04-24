#version 450

layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform SkyboxUBO
{
  mat4 world_to_clip;
  vec4 light_position;
  float time_seconds;
  vec3 _pad;
} ubo;

vec3 getSunColor(float sunHeight)
{
  vec3 noonSun = vec3(1.0, 0.6, 0.0);
  vec3 sunsetSun = vec3(1.0, 0.7, 0.3);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  if (sunHeight > 0.2)
  {
    float t = (sunHeight - 0.2) / 0.8;
    return mix(sunsetSun, noonSun, t);
  }
  else if (sunHeight > -0.1)
  {
    float t = (sunHeight + 0.1) / 0.3;
    return mix(nightSun, sunsetSun, t);
  }
  return nightSun;
}

float hash12(vec2 p)
{
  p = fract(p * vec2(5.3983, 5.4427));
  p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
  return fract(p.x * p.y * 95.4337);
}

vec2 hash22(vec2 p)
{
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

vec3 LayerStars(vec2 uv, float scale, float seed)
{
  vec2 gridUv = uv * scale;
  vec2 gridId = floor(gridUv);
  vec2 localUv = fract(gridUv) - 0.5;

  vec3 layerColor = vec3(0.0);

  for (int y = -1; y <= 1; y++)
  {
    for (int x = -1; x <= 1; x++)
    {
      vec2 neighborOffset = vec2(float(x), float(y));
      vec2 neighborGridId = gridId + neighborOffset;
      vec2 idWithSeed = neighborGridId + vec2(seed, seed * 12.34);

      vec2 starOffset = hash22(idWithSeed) * 0.4;
      vec2 starPosInLocal = neighborOffset + starOffset;

      float dist = length(localUv - starPosInLocal);

      float brightness = 1.0 - smoothstep(0.0, 0.1, dist);
      brightness = pow(brightness, 5.0);

      float randomVal = hash12(idWithSeed);
      float twinkleSpeed = 6.0 + randomVal * 4.0;
      float timeOffset = randomVal * 6.2831;
      float twinkle = sin(ubo.time_seconds * twinkleSpeed + timeOffset) * 0.35 + 0.65;
      vec3 starTint = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.9, 0.7), randomVal);

      layerColor += starTint * brightness * twinkle;
    }
  }
  return layerColor;
}

vec2 DirToRectilinearUV(vec3 dir)
{
  vec3 absDir = abs(dir);
  float ma;
  vec2 uv;

  if (absDir.z >= absDir.x && absDir.z >= absDir.y)
  {
    ma = absDir.z;
    uv = dir.xy;
  }
  else if (absDir.y >= absDir.x)
  {
    ma = absDir.y;
    uv = dir.xz;
  }
  else
  {
    ma = absDir.x;
    uv = dir.yz;
  }

  return uv / ma;
}

void main()
{
  vec3 V = normalize(localPos);
  vec3 L = normalize(ubo.light_position.xyz);

  vec3 sunDiskColor = getSunColor(L.y);
  float sunHeight = L.y;

  vec3 noonTop = vec3(0.2, 0.5, 0.9);
  vec3 noonHoriz = vec3(0.6, 0.8, 1.0);

  vec3 sunsetTop = vec3(0.2, 0.2, 0.4);
  vec3 sunsetHoriz = vec3(1.0, 0.5, 0.1);

  vec3 nightTop = vec3(0.0, 0.0, 0.1);
  vec3 nightHoriz = vec3(0.0, 0.0, 0.2);

  vec3 skyTop;
  vec3 skyHoriz;
  int isNight = 0;
  if (sunHeight > 0.2)
  {
    float t = clamp((sunHeight - 0.2) / 0.8, 0.0, 1.0);
    skyTop = mix(sunsetTop, noonTop, t);
    skyHoriz = mix(sunsetHoriz, noonHoriz, t);
  }
  else if (sunHeight > -0.2)
  {
    float t = clamp((sunHeight + 0.2) / 0.4, 0.0, 1.0);
    skyTop = mix(nightTop, sunsetTop, t);
    skyHoriz = mix(nightHoriz, sunsetHoriz, t);
  }
  else
  {
    skyTop = nightTop;
    skyHoriz = nightHoriz;
    isNight = 1;
  }

  float gradient = clamp(V.y, 0.0, 1.0);
  gradient = pow(gradient, 0.5);

  float sunDot = dot(V, L);
  float sunMask = step(0.999, sunDot);
  vec3 skyColor = mix(skyHoriz, skyTop, gradient) * (1.0 - sunMask);

  vec3 dir = normalize(localPos);
  vec2 skyUV = DirToRectilinearUV(dir) * 2.0;
  vec3 starColor = vec3(0.0);
  starColor += LayerStars(skyUV, 10.0, 0.0);
  starColor += LayerStars(skyUV, 20.0, 12.34) * 0.7;
  starColor += LayerStars(skyUV, 40.0, 56.78) * 0.5;

  vec3 bgColor = mix(vec3(0.005, 0.01, 0.04), vec3(0.02, 0.03, 0.08), smoothstep(-0.5, 0.5, dir.y));
  starColor += bgColor;

  skyColor += sunMask * sunDiskColor;
  skyColor += starColor * (1.0 - smoothstep(-0.2, 0.0, sunHeight));

  out_color = vec4(skyColor, 1.0);
}

