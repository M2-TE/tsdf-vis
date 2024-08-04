// SMAA portability defines
#define SMAATexture2DMS2(tex) sampler2DMS tex
#define SMAALoad(tex, pos, sample) texelFetch(tex, pos, sample)
#define SMAA_AREATEX_SELECT(sample) sample.rg
#define SMAA_SEARCHTEX_SELECT(sample) sample.r
// SMAA settings:
#define SMAA_RT_METRICS vec4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0) // todo: make dynamic
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"