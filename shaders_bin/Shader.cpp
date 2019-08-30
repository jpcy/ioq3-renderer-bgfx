#include "Bloom_fragment.h"
#include "Color_fragment.h"
#include "Depth_fragment.h"
#include "Fog_fragment.h"
#include "GaussianBlur_fragment.h"
#include "Generic_fragment.h"
#include "SMAABlendingWeightCalculation_fragment.h"
#include "SMAAEdgeDetection_fragment.h"
#include "SMAANeighborhoodBlending_fragment.h"
#include "Texture_fragment.h"
#include "TextureColor_fragment.h"
#include "TextureDebug_fragment.h"
#include "TextureVariation_fragment.h"
#include "Color_vertex.h"
#include "Depth_vertex.h"
#include "Fog_vertex.h"
#include "Generic_vertex.h"
#include "SMAABlendingWeightCalculation_vertex.h"
#include "SMAAEdgeDetection_vertex.h"
#include "SMAANeighborhoodBlending_vertex.h"
#include "Texture_vertex.h"

struct ShaderSourceMem { const uint8_t *mem; size_t size; };

static std::array<ShaderSourceMem, FragmentShaderId::Num> GetFragmentShaderSourceMap_gl()
{
	std::array<ShaderSourceMem, FragmentShaderId::Num> mem;
	mem[FragmentShaderId::Bloom].mem = Bloom_fragment_gl;
	mem[FragmentShaderId::Bloom].size = sizeof(Bloom_fragment_gl);
	mem[FragmentShaderId::Color].mem = Color_fragment_gl;
	mem[FragmentShaderId::Color].size = sizeof(Color_fragment_gl);
	mem[FragmentShaderId::Depth].mem = Depth_fragment_gl;
	mem[FragmentShaderId::Depth].size = sizeof(Depth_fragment_gl);
	mem[FragmentShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_fragment_gl;
	mem[FragmentShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_fragment_gl);
	mem[FragmentShaderId::Fog].mem = Fog_fragment_gl;
	mem[FragmentShaderId::Fog].size = sizeof(Fog_fragment_gl);
	mem[FragmentShaderId::GaussianBlur].mem = GaussianBlur_fragment_gl;
	mem[FragmentShaderId::GaussianBlur].size = sizeof(GaussianBlur_fragment_gl);
	mem[FragmentShaderId::Generic].mem = Generic_fragment_gl;
	mem[FragmentShaderId::Generic].size = sizeof(Generic_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTest].mem = Generic_AlphaTest_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTest].size = sizeof(Generic_AlphaTest_fragment_gl);
	mem[FragmentShaderId::Generic_DynamicLights].mem = Generic_DynamicLights_fragment_gl;
	mem[FragmentShaderId::Generic_DynamicLights].size = sizeof(Generic_DynamicLights_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].mem = Generic_AlphaTestDynamicLights_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].size = sizeof(Generic_AlphaTestDynamicLights_fragment_gl);
	mem[FragmentShaderId::Generic_SoftSprite].mem = Generic_SoftSprite_fragment_gl;
	mem[FragmentShaderId::Generic_SoftSprite].size = sizeof(Generic_SoftSprite_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].mem = Generic_AlphaTestSoftSprite_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].size = sizeof(Generic_AlphaTestSoftSprite_fragment_gl);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].mem = Generic_DynamicLightsSoftSprite_fragment_gl;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].size = sizeof(Generic_DynamicLightsSoftSprite_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].mem = Generic_AlphaTestDynamicLightsSoftSprite_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].size = sizeof(Generic_AlphaTestDynamicLightsSoftSprite_fragment_gl);
	mem[FragmentShaderId::Generic_SunLight].mem = Generic_SunLight_fragment_gl;
	mem[FragmentShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestSunLight].mem = Generic_AlphaTestSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestSunLight].size = sizeof(Generic_AlphaTestSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].mem = Generic_DynamicLightsSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].size = sizeof(Generic_DynamicLightsSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].mem = Generic_AlphaTestDynamicLightsSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].mem = Generic_SoftSpriteSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].size = sizeof(Generic_SoftSpriteSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].mem = Generic_AlphaTestSoftSpriteSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].size = sizeof(Generic_AlphaTestSoftSpriteSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].mem = Generic_DynamicLightsSoftSpriteSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].size = sizeof(Generic_DynamicLightsSoftSpriteSunLight_fragment_gl);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].mem = Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_gl;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_gl);
	mem[FragmentShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_fragment_gl;
	mem[FragmentShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_fragment_gl);
	mem[FragmentShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_fragment_gl;
	mem[FragmentShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_fragment_gl);
	mem[FragmentShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_fragment_gl;
	mem[FragmentShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_fragment_gl);
	mem[FragmentShaderId::Texture].mem = Texture_fragment_gl;
	mem[FragmentShaderId::Texture].size = sizeof(Texture_fragment_gl);
	mem[FragmentShaderId::TextureColor].mem = TextureColor_fragment_gl;
	mem[FragmentShaderId::TextureColor].size = sizeof(TextureColor_fragment_gl);
	mem[FragmentShaderId::TextureDebug].mem = TextureDebug_fragment_gl;
	mem[FragmentShaderId::TextureDebug].size = sizeof(TextureDebug_fragment_gl);
	mem[FragmentShaderId::TextureVariation].mem = TextureVariation_fragment_gl;
	mem[FragmentShaderId::TextureVariation].size = sizeof(TextureVariation_fragment_gl);
	mem[FragmentShaderId::TextureVariation_SunLight].mem = TextureVariation_SunLight_fragment_gl;
	mem[FragmentShaderId::TextureVariation_SunLight].size = sizeof(TextureVariation_SunLight_fragment_gl);
	return mem;
}

static std::array<ShaderSourceMem, VertexShaderId::Num> GetVertexShaderSourceMap_gl()
{
	std::array<ShaderSourceMem, VertexShaderId::Num> mem;
	mem[VertexShaderId::Color].mem = Color_vertex_gl;
	mem[VertexShaderId::Color].size = sizeof(Color_vertex_gl);
	mem[VertexShaderId::Depth].mem = Depth_vertex_gl;
	mem[VertexShaderId::Depth].size = sizeof(Depth_vertex_gl);
	mem[VertexShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_vertex_gl;
	mem[VertexShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_vertex_gl);
	mem[VertexShaderId::Fog].mem = Fog_vertex_gl;
	mem[VertexShaderId::Fog].size = sizeof(Fog_vertex_gl);
	mem[VertexShaderId::Generic].mem = Generic_vertex_gl;
	mem[VertexShaderId::Generic].size = sizeof(Generic_vertex_gl);
	mem[VertexShaderId::Generic_SunLight].mem = Generic_SunLight_vertex_gl;
	mem[VertexShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_vertex_gl);
	mem[VertexShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_vertex_gl;
	mem[VertexShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_vertex_gl);
	mem[VertexShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_vertex_gl;
	mem[VertexShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_vertex_gl);
	mem[VertexShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_vertex_gl;
	mem[VertexShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_vertex_gl);
	mem[VertexShaderId::Texture].mem = Texture_vertex_gl;
	mem[VertexShaderId::Texture].size = sizeof(Texture_vertex_gl);
	return mem;
}

static std::array<ShaderSourceMem, FragmentShaderId::Num> GetFragmentShaderSourceMap_d3d11()
{
	std::array<ShaderSourceMem, FragmentShaderId::Num> mem;
	mem[FragmentShaderId::Bloom].mem = Bloom_fragment_d3d11;
	mem[FragmentShaderId::Bloom].size = sizeof(Bloom_fragment_d3d11);
	mem[FragmentShaderId::Color].mem = Color_fragment_d3d11;
	mem[FragmentShaderId::Color].size = sizeof(Color_fragment_d3d11);
	mem[FragmentShaderId::Depth].mem = Depth_fragment_d3d11;
	mem[FragmentShaderId::Depth].size = sizeof(Depth_fragment_d3d11);
	mem[FragmentShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_fragment_d3d11;
	mem[FragmentShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_fragment_d3d11);
	mem[FragmentShaderId::Fog].mem = Fog_fragment_d3d11;
	mem[FragmentShaderId::Fog].size = sizeof(Fog_fragment_d3d11);
	mem[FragmentShaderId::GaussianBlur].mem = GaussianBlur_fragment_d3d11;
	mem[FragmentShaderId::GaussianBlur].size = sizeof(GaussianBlur_fragment_d3d11);
	mem[FragmentShaderId::Generic].mem = Generic_fragment_d3d11;
	mem[FragmentShaderId::Generic].size = sizeof(Generic_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTest].mem = Generic_AlphaTest_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTest].size = sizeof(Generic_AlphaTest_fragment_d3d11);
	mem[FragmentShaderId::Generic_DynamicLights].mem = Generic_DynamicLights_fragment_d3d11;
	mem[FragmentShaderId::Generic_DynamicLights].size = sizeof(Generic_DynamicLights_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].mem = Generic_AlphaTestDynamicLights_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].size = sizeof(Generic_AlphaTestDynamicLights_fragment_d3d11);
	mem[FragmentShaderId::Generic_SoftSprite].mem = Generic_SoftSprite_fragment_d3d11;
	mem[FragmentShaderId::Generic_SoftSprite].size = sizeof(Generic_SoftSprite_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].mem = Generic_AlphaTestSoftSprite_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].size = sizeof(Generic_AlphaTestSoftSprite_fragment_d3d11);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].mem = Generic_DynamicLightsSoftSprite_fragment_d3d11;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].size = sizeof(Generic_DynamicLightsSoftSprite_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].mem = Generic_AlphaTestDynamicLightsSoftSprite_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].size = sizeof(Generic_AlphaTestDynamicLightsSoftSprite_fragment_d3d11);
	mem[FragmentShaderId::Generic_SunLight].mem = Generic_SunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestSunLight].mem = Generic_AlphaTestSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestSunLight].size = sizeof(Generic_AlphaTestSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].mem = Generic_DynamicLightsSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].size = sizeof(Generic_DynamicLightsSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].mem = Generic_AlphaTestDynamicLightsSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].mem = Generic_SoftSpriteSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].size = sizeof(Generic_SoftSpriteSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].mem = Generic_AlphaTestSoftSpriteSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].size = sizeof(Generic_AlphaTestSoftSpriteSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].mem = Generic_DynamicLightsSoftSpriteSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].size = sizeof(Generic_DynamicLightsSoftSpriteSunLight_fragment_d3d11);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].mem = Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_d3d11;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_d3d11);
	mem[FragmentShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_fragment_d3d11;
	mem[FragmentShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_fragment_d3d11);
	mem[FragmentShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_fragment_d3d11;
	mem[FragmentShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_fragment_d3d11);
	mem[FragmentShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_fragment_d3d11;
	mem[FragmentShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_fragment_d3d11);
	mem[FragmentShaderId::Texture].mem = Texture_fragment_d3d11;
	mem[FragmentShaderId::Texture].size = sizeof(Texture_fragment_d3d11);
	mem[FragmentShaderId::TextureColor].mem = TextureColor_fragment_d3d11;
	mem[FragmentShaderId::TextureColor].size = sizeof(TextureColor_fragment_d3d11);
	mem[FragmentShaderId::TextureDebug].mem = TextureDebug_fragment_d3d11;
	mem[FragmentShaderId::TextureDebug].size = sizeof(TextureDebug_fragment_d3d11);
	mem[FragmentShaderId::TextureVariation].mem = TextureVariation_fragment_d3d11;
	mem[FragmentShaderId::TextureVariation].size = sizeof(TextureVariation_fragment_d3d11);
	mem[FragmentShaderId::TextureVariation_SunLight].mem = TextureVariation_SunLight_fragment_d3d11;
	mem[FragmentShaderId::TextureVariation_SunLight].size = sizeof(TextureVariation_SunLight_fragment_d3d11);
	return mem;
}

static std::array<ShaderSourceMem, VertexShaderId::Num> GetVertexShaderSourceMap_d3d11()
{
	std::array<ShaderSourceMem, VertexShaderId::Num> mem;
	mem[VertexShaderId::Color].mem = Color_vertex_d3d11;
	mem[VertexShaderId::Color].size = sizeof(Color_vertex_d3d11);
	mem[VertexShaderId::Depth].mem = Depth_vertex_d3d11;
	mem[VertexShaderId::Depth].size = sizeof(Depth_vertex_d3d11);
	mem[VertexShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_vertex_d3d11;
	mem[VertexShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_vertex_d3d11);
	mem[VertexShaderId::Fog].mem = Fog_vertex_d3d11;
	mem[VertexShaderId::Fog].size = sizeof(Fog_vertex_d3d11);
	mem[VertexShaderId::Generic].mem = Generic_vertex_d3d11;
	mem[VertexShaderId::Generic].size = sizeof(Generic_vertex_d3d11);
	mem[VertexShaderId::Generic_SunLight].mem = Generic_SunLight_vertex_d3d11;
	mem[VertexShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_vertex_d3d11);
	mem[VertexShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_vertex_d3d11;
	mem[VertexShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_vertex_d3d11);
	mem[VertexShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_vertex_d3d11;
	mem[VertexShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_vertex_d3d11);
	mem[VertexShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_vertex_d3d11;
	mem[VertexShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_vertex_d3d11);
	mem[VertexShaderId::Texture].mem = Texture_vertex_d3d11;
	mem[VertexShaderId::Texture].size = sizeof(Texture_vertex_d3d11);
	return mem;
}

static std::array<ShaderSourceMem, FragmentShaderId::Num> GetFragmentShaderSourceMap_vk()
{
	std::array<ShaderSourceMem, FragmentShaderId::Num> mem;
	mem[FragmentShaderId::Bloom].mem = Bloom_fragment_vk;
	mem[FragmentShaderId::Bloom].size = sizeof(Bloom_fragment_vk);
	mem[FragmentShaderId::Color].mem = Color_fragment_vk;
	mem[FragmentShaderId::Color].size = sizeof(Color_fragment_vk);
	mem[FragmentShaderId::Depth].mem = Depth_fragment_vk;
	mem[FragmentShaderId::Depth].size = sizeof(Depth_fragment_vk);
	mem[FragmentShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_fragment_vk;
	mem[FragmentShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_fragment_vk);
	mem[FragmentShaderId::Fog].mem = Fog_fragment_vk;
	mem[FragmentShaderId::Fog].size = sizeof(Fog_fragment_vk);
	mem[FragmentShaderId::GaussianBlur].mem = GaussianBlur_fragment_vk;
	mem[FragmentShaderId::GaussianBlur].size = sizeof(GaussianBlur_fragment_vk);
	mem[FragmentShaderId::Generic].mem = Generic_fragment_vk;
	mem[FragmentShaderId::Generic].size = sizeof(Generic_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTest].mem = Generic_AlphaTest_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTest].size = sizeof(Generic_AlphaTest_fragment_vk);
	mem[FragmentShaderId::Generic_DynamicLights].mem = Generic_DynamicLights_fragment_vk;
	mem[FragmentShaderId::Generic_DynamicLights].size = sizeof(Generic_DynamicLights_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].mem = Generic_AlphaTestDynamicLights_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLights].size = sizeof(Generic_AlphaTestDynamicLights_fragment_vk);
	mem[FragmentShaderId::Generic_SoftSprite].mem = Generic_SoftSprite_fragment_vk;
	mem[FragmentShaderId::Generic_SoftSprite].size = sizeof(Generic_SoftSprite_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].mem = Generic_AlphaTestSoftSprite_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestSoftSprite].size = sizeof(Generic_AlphaTestSoftSprite_fragment_vk);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].mem = Generic_DynamicLightsSoftSprite_fragment_vk;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSprite].size = sizeof(Generic_DynamicLightsSoftSprite_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].mem = Generic_AlphaTestDynamicLightsSoftSprite_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSprite].size = sizeof(Generic_AlphaTestDynamicLightsSoftSprite_fragment_vk);
	mem[FragmentShaderId::Generic_SunLight].mem = Generic_SunLight_fragment_vk;
	mem[FragmentShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestSunLight].mem = Generic_AlphaTestSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestSunLight].size = sizeof(Generic_AlphaTestSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].mem = Generic_DynamicLightsSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_DynamicLightsSunLight].size = sizeof(Generic_DynamicLightsSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].mem = Generic_AlphaTestDynamicLightsSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].mem = Generic_SoftSpriteSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_SoftSpriteSunLight].size = sizeof(Generic_SoftSpriteSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].mem = Generic_AlphaTestSoftSpriteSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestSoftSpriteSunLight].size = sizeof(Generic_AlphaTestSoftSpriteSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].mem = Generic_DynamicLightsSoftSpriteSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_DynamicLightsSoftSpriteSunLight].size = sizeof(Generic_DynamicLightsSoftSpriteSunLight_fragment_vk);
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].mem = Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_vk;
	mem[FragmentShaderId::Generic_AlphaTestDynamicLightsSoftSpriteSunLight].size = sizeof(Generic_AlphaTestDynamicLightsSoftSpriteSunLight_fragment_vk);
	mem[FragmentShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_fragment_vk;
	mem[FragmentShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_fragment_vk);
	mem[FragmentShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_fragment_vk;
	mem[FragmentShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_fragment_vk);
	mem[FragmentShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_fragment_vk;
	mem[FragmentShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_fragment_vk);
	mem[FragmentShaderId::Texture].mem = Texture_fragment_vk;
	mem[FragmentShaderId::Texture].size = sizeof(Texture_fragment_vk);
	mem[FragmentShaderId::TextureColor].mem = TextureColor_fragment_vk;
	mem[FragmentShaderId::TextureColor].size = sizeof(TextureColor_fragment_vk);
	mem[FragmentShaderId::TextureDebug].mem = TextureDebug_fragment_vk;
	mem[FragmentShaderId::TextureDebug].size = sizeof(TextureDebug_fragment_vk);
	mem[FragmentShaderId::TextureVariation].mem = TextureVariation_fragment_vk;
	mem[FragmentShaderId::TextureVariation].size = sizeof(TextureVariation_fragment_vk);
	mem[FragmentShaderId::TextureVariation_SunLight].mem = TextureVariation_SunLight_fragment_vk;
	mem[FragmentShaderId::TextureVariation_SunLight].size = sizeof(TextureVariation_SunLight_fragment_vk);
	return mem;
}

static std::array<ShaderSourceMem, VertexShaderId::Num> GetVertexShaderSourceMap_vk()
{
	std::array<ShaderSourceMem, VertexShaderId::Num> mem;
	mem[VertexShaderId::Color].mem = Color_vertex_vk;
	mem[VertexShaderId::Color].size = sizeof(Color_vertex_vk);
	mem[VertexShaderId::Depth].mem = Depth_vertex_vk;
	mem[VertexShaderId::Depth].size = sizeof(Depth_vertex_vk);
	mem[VertexShaderId::Depth_AlphaTest].mem = Depth_AlphaTest_vertex_vk;
	mem[VertexShaderId::Depth_AlphaTest].size = sizeof(Depth_AlphaTest_vertex_vk);
	mem[VertexShaderId::Fog].mem = Fog_vertex_vk;
	mem[VertexShaderId::Fog].size = sizeof(Fog_vertex_vk);
	mem[VertexShaderId::Generic].mem = Generic_vertex_vk;
	mem[VertexShaderId::Generic].size = sizeof(Generic_vertex_vk);
	mem[VertexShaderId::Generic_SunLight].mem = Generic_SunLight_vertex_vk;
	mem[VertexShaderId::Generic_SunLight].size = sizeof(Generic_SunLight_vertex_vk);
	mem[VertexShaderId::SMAABlendingWeightCalculation].mem = SMAABlendingWeightCalculation_vertex_vk;
	mem[VertexShaderId::SMAABlendingWeightCalculation].size = sizeof(SMAABlendingWeightCalculation_vertex_vk);
	mem[VertexShaderId::SMAAEdgeDetection].mem = SMAAEdgeDetection_vertex_vk;
	mem[VertexShaderId::SMAAEdgeDetection].size = sizeof(SMAAEdgeDetection_vertex_vk);
	mem[VertexShaderId::SMAANeighborhoodBlending].mem = SMAANeighborhoodBlending_vertex_vk;
	mem[VertexShaderId::SMAANeighborhoodBlending].size = sizeof(SMAANeighborhoodBlending_vertex_vk);
	mem[VertexShaderId::Texture].mem = Texture_vertex_vk;
	mem[VertexShaderId::Texture].size = sizeof(Texture_vertex_vk);
	return mem;
}
