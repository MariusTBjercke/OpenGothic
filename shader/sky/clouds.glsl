#ifndef CLOUDS_GLSL
#define CLOUDS_GLSL

vec4 clouds(vec3 at, float nightPhase, vec3 highlight,
            vec2 dxy0, vec2 dxy1,
            in sampler2D dayL1,   in sampler2D dayL0,
            in sampler2D nightL1, in sampler2D nightL0) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

#if defined(SKY_LOD)
  vec4  cloudDL1 = textureLod(dayL1,   texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudDL0 = textureLod(dayL0,   texc*0.3 + dxy0, SKY_LOD);
  vec4  cloudNL1 = textureLod(nightL1, texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudNL0 = textureLod(nightL0, texc*0.6 + vec2(0.5), SKY_LOD); // stars
#else
  vec4  cloudDL1 = texture(dayL1,   texc*0.3 + dxy1);
  vec4  cloudDL0 = texture(dayL0,   texc*0.3 + dxy0);
  vec4  cloudNL1 = texture(nightL1, texc*0.3 + dxy1);
  vec4  cloudNL0 = texture(nightL0, texc*0.6 + vec2(0.5)); // stars
#endif

  cloudDL0.a   = cloudDL0.a*0.2;
  cloudDL1.a   = cloudDL1.a*0.2;
  cloudNL0.a   = cloudNL0.a*1.0; // stars
  cloudNL1.a   = cloudNL1.a*0.1;

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb);
  night.rgb = srgbDecode(night.rgb);

  day.rgb   = day.rgb  *highlight*5.0;
  night.rgb = night.rgb*0.000014;

  day  .a   = day  .a*(1.0-nightPhase);
  //night.a   = night.a*(nightPhase);

  vec4 color = mixClr(day,night);
  // color.rgb += hday;

  return color;
  }

// fork: brightness of the rain overcast relative to the average horizon sky radiance. The overcast also feeds the
// sky irradiance (ambient light), so it decides how bright the scene gets in rain
const float RainOvercastBrightness = 0.3;

// fork: rain overcast, a grey layer lit by the average horizon sky radiance, shaped by the original rain cloud
// texture (SKYRAINCLOUDS.TGA) in two layers of different scale and drift. The texture only varies the brightness
// around its own average, so the overall brightness (and the scene light it gives) stays the same.
// The radiance is sampled in fixed directions: samples that follow the view direction form a cross at the zenith
float rainCloudDensity(in sampler2D rainL, vec2 uv) {
#if defined(SKY_LOD)
  vec3 c = textureLod(rainL, uv, SKY_LOD).rgb;
#else
  vec3 c = texture(rainL, uv).rgb;
#endif
  return dot(srgbDecode(c), vec3(0.2125, 0.7154, 0.0721));
  }

vec3 rainOvercast(vec3 at, in sampler2D skyLUT, vec3 plPos, vec3 sunDir, vec2 dxy0, vec2 dxy1, in sampler2D rainL) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

  const vec3  avgC  = textureLod(rainL, vec2(0.5), float(textureQueryLevels(rainL)-1)).rgb;
  const float avg   = max(dot(srgbDecode(avgC), vec3(0.2125, 0.7154, 0.0721)), 0.001);
  const float d0    = rainCloudDensity(rainL, texc*0.12 + dxy0*0.8);
  const float d1    = rainCloudDensity(rainL, texc*0.27 + dxy1*1.4 + vec2(0.37, 0.61));
  const float shade = clamp(mix(d0, d1, 0.4)/avg, 0.35, 1.8);
  vec3  lum      = vec3(0);
  lum += textureSkyLUT(skyLUT, plPos, vec3( 1,0, 0), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3(-1,0, 0), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3( 0,0, 1), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3( 0,0,-1), sunDir);
  float grey     = dot(lum*0.25, vec3(0.2125, 0.7154, 0.0721)) * RainOvercastBrightness;
  // 0.85: the previous day-texture version averaged about that
  return vec3(grey) * 0.85 * mix(1.0, shade, 0.8);
  }

vec3 applyClouds(vec3 skyColor, in sampler2D skyLUT, vec3 plPos, vec3 sunDir, vec3 view, float nightPhase,
                 vec2 dxy0, vec2 dxy1,
                 in sampler2D dayL1,   in sampler2D dayL0,
                 in sampler2D nightL1, in sampler2D nightL0, in sampler2D rainL, float rain) {
  float L = rayIntersect(plPos, view, RClouds);
  // TODO: http://killzone.dl.playstation.net/killzone/horizonzerodawn/presentations/Siggraph15_Schneider_Real-Time_Volumetric_Cloudscapes_of_Horizon_Zero_Dawn.pdf
  // fake cloud scattering inspired by Henyey-Greenstein model
  vec3 lum  = vec3(0);
  lum += textureSkyLUT(skyLUT, plPos, vec3( view.x, view.y*0.0, view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3(-view.x, view.y*0.0, view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3(-view.x, view.y*0.0,-view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3( view.x, view.y*0.0,-view.z), sunDir);
  //return lum;

  vec4 cloud = clouds((plPos + view*L)*0.01, nightPhase, lum,
                      dxy0, dxy1,
                      dayL1,dayL0, nightL1,nightL0);
  vec3 ret   = skyColor + cloud.rgb * cloud.a;
  if(rain>0.0)
    ret = mix(ret, rainOvercast((plPos + view*L)*0.01, skyLUT, plPos, sunDir, dxy0, dxy1, rainL), rainCover(rain));
  return ret;
  // return mix(skyColor, cloud.rgb, cloud.a);
  }

#endif
