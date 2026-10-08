#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// fork: wet surfaces while and after it rains. Only surfaces rain can reach are wet: the rain map holds the height of
// the topmost static surface per cell around the camera (common/world/weather.cpp), anything clearly below it is
// under a roof.
//  default: multiplied into the G-buffer albedo (blend DstColor/Zero) before lighting, darker surfaces
//  SHEEN:   added to the lit scene (blend One/One), a reflection of the (overcast) sky on wet, up-facing surfaces

#include "scene.glsl"
#include "common.glsl"
#if defined(SHEEN)
#include "sky/sky_common.glsl"
#endif

layout(push_constant, std430) uniform UboPush {
  vec2  origin;  // world x/z of cell (0,0) corner, cm
  float cell;    // cell size, cm
  int   size;    // cells per side
  float wetness; // 0..1
  } push;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform usampler2D gbufNormal;
layout(binding = 2) uniform sampler2D  depth;
layout(binding = 3, std430) readonly buffer RainMap {
  float height[];
  };
#if defined(SHEEN)
layout(binding = 4) uniform sampler2D skyLUT;
#endif

layout(location = 0) out vec4 outColor;

const float Unknown = 1e8; // not traced yet: count as covered
const float Open    = -1e8; // nothing above

float cellHeight(ivec2 c) {
  if(any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, ivec2(push.size))))
    return Open;
  return height[c.y*push.size + c.x];
  }

// height of the topmost surface at xz: bilinear between cell centers where all four are traced, else nearest
float topHeight(vec2 xz) {
  const vec2  at = (xz - push.origin)/push.cell - 0.5;
  const ivec2 c  = ivec2(floor(at));
  const vec2  f  = at - vec2(c);

  const float h00 = cellHeight(c);
  const float h10 = cellHeight(c + ivec2(1,0));
  const float h01 = cellHeight(c + ivec2(0,1));
  const float h11 = cellHeight(c + ivec2(1,1));
  if(max(max(abs(h00),abs(h10)), max(abs(h01),abs(h11))) < 1e7)
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
  return cellHeight(ivec2(floor(at + 0.5)));
  }

void main() {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const float d         = texelFetch(depth, fragCoord, 0).r;
  if(d==1.0)
    discard;

  const vec2  uv     = vec2(fragCoord)*scene.screenResInv*2.0 - vec2(1.0);
  const vec4  pos4   = scene.viewProjectInv * vec4(uv, d, 1.0);
  const vec3  pos    = pos4.xyz/pos4.w;
  const vec3  normal = normalFetch(gbufNormal, fragCoord);

  // up to 40 cm below the top surface still counts as reached (cell size, slopes); fully dry 1 m below it
  const float top     = topHeight(pos.xz);
  const float exposed = smoothstep(top-100.0, top-40.0, pos.y);
  const float facing  = mix(0.35, 1.0, clamp(normal.y, 0.0, 1.0));

#if defined(SHEEN)
  // a thin water film: only on surfaces facing up, reflecting the sky with the Fresnel term of water
  const float film = push.wetness * exposed * smoothstep(0.5, 0.9, normal.y);
  if(film<=0.001)
    discard;
  const vec3  view = normalize(pos - scene.camPos);
  vec3        refl = reflect(view, normal);
  refl.y = max(refl.y, 0.0);
  refl   = normalize(refl);
  const float fr  = fresnel(refl, normal, IorWater);
  const vec3  sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir) * scene.GSunIntensity * scene.exposure;
  outColor = vec4(sky * fr * film * 0.6, 0.0);
#else
  // gbuffer albedo is gamma encoded: 0.3 here is about half the linear albedo
  const float k = push.wetness * exposed * facing;
  const float f = 1.0 - 0.3*k;
  outColor = vec4(f, f, f, 1.0);
#endif
  }
