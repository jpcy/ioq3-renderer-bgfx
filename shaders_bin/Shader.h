struct FragmentShaderId
{
	enum Enum
	{
		Bloom,
		Color,
		Depth,
		Depth_AlphaTest,
		Fog,
		Fog_Bloom,
		GaussianBlur,
		Generic,
		Generic_AlphaTest,
		Generic_Bloom,
		Generic_AlphaTestBloom,
		Generic_DynamicLights,
		Generic_AlphaTestDynamicLights,
		Generic_BloomDynamicLights,
		Generic_AlphaTestBloomDynamicLights,
		Generic_SoftSprite,
		Generic_AlphaTestSoftSprite,
		Generic_BloomSoftSprite,
		Generic_AlphaTestBloomSoftSprite,
		Generic_DynamicLightsSoftSprite,
		Generic_AlphaTestDynamicLightsSoftSprite,
		Generic_BloomDynamicLightsSoftSprite,
		Generic_AlphaTestBloomDynamicLightsSoftSprite,
		Generic_SunLight,
		Generic_AlphaTestSunLight,
		Generic_BloomSunLight,
		Generic_AlphaTestBloomSunLight,
		Generic_DynamicLightsSunLight,
		Generic_AlphaTestDynamicLightsSunLight,
		Generic_BloomDynamicLightsSunLight,
		Generic_AlphaTestBloomDynamicLightsSunLight,
		Generic_SoftSpriteSunLight,
		Generic_AlphaTestSoftSpriteSunLight,
		Generic_BloomSoftSpriteSunLight,
		Generic_AlphaTestBloomSoftSpriteSunLight,
		Generic_DynamicLightsSoftSpriteSunLight,
		Generic_AlphaTestDynamicLightsSoftSpriteSunLight,
		Generic_BloomDynamicLightsSoftSpriteSunLight,
		Generic_AlphaTestBloomDynamicLightsSoftSpriteSunLight,
		SMAABlendingWeightCalculation,
		SMAAEdgeDetection,
		SMAANeighborhoodBlending,
		Texture,
		TextureColor,
		TextureDebug,
		TextureVariation,
		TextureVariation_Bloom,
		TextureVariation_SunLight,
		TextureVariation_BloomSunLight,
		Num
	};
};

#ifdef _DEBUG
static const char *s_fragmentShaderNames[] =
{
	"Bloom",
	"Color",
	"Depth",
	"Depth_AlphaTest",
	"Fog",
	"Fog_Bloom",
	"GaussianBlur",
	"Generic",
	"Generic_AlphaTest",
	"Generic_Bloom",
	"Generic_AlphaTestBloom",
	"Generic_DynamicLights",
	"Generic_AlphaTestDynamicLights",
	"Generic_BloomDynamicLights",
	"Generic_AlphaTestBloomDynamicLights",
	"Generic_SoftSprite",
	"Generic_AlphaTestSoftSprite",
	"Generic_BloomSoftSprite",
	"Generic_AlphaTestBloomSoftSprite",
	"Generic_DynamicLightsSoftSprite",
	"Generic_AlphaTestDynamicLightsSoftSprite",
	"Generic_BloomDynamicLightsSoftSprite",
	"Generic_AlphaTestBloomDynamicLightsSoftSprite",
	"Generic_SunLight",
	"Generic_AlphaTestSunLight",
	"Generic_BloomSunLight",
	"Generic_AlphaTestBloomSunLight",
	"Generic_DynamicLightsSunLight",
	"Generic_AlphaTestDynamicLightsSunLight",
	"Generic_BloomDynamicLightsSunLight",
	"Generic_AlphaTestBloomDynamicLightsSunLight",
	"Generic_SoftSpriteSunLight",
	"Generic_AlphaTestSoftSpriteSunLight",
	"Generic_BloomSoftSpriteSunLight",
	"Generic_AlphaTestBloomSoftSpriteSunLight",
	"Generic_DynamicLightsSoftSpriteSunLight",
	"Generic_AlphaTestDynamicLightsSoftSpriteSunLight",
	"Generic_BloomDynamicLightsSoftSpriteSunLight",
	"Generic_AlphaTestBloomDynamicLightsSoftSpriteSunLight",
	"SMAABlendingWeightCalculation",
	"SMAAEdgeDetection",
	"SMAANeighborhoodBlending",
	"Texture",
	"TextureColor",
	"TextureDebug",
	"TextureVariation",
	"TextureVariation_Bloom",
	"TextureVariation_SunLight",
	"TextureVariation_BloomSunLight",
};
#endif

struct VertexShaderId
{
	enum Enum
	{
		Color,
		Depth,
		Depth_AlphaTest,
		Fog,
		Generic,
		Generic_SunLight,
		SMAABlendingWeightCalculation,
		SMAAEdgeDetection,
		SMAANeighborhoodBlending,
		Texture,
		Num
	};
};

#ifdef _DEBUG
static const char *s_vertexShaderNames[] =
{
	"Color",
	"Depth",
	"Depth_AlphaTest",
	"Fog",
	"Generic",
	"Generic_SunLight",
	"SMAABlendingWeightCalculation",
	"SMAAEdgeDetection",
	"SMAANeighborhoodBlending",
	"Texture",
};
#endif

struct GenericFragmentShaderVariant
{
	enum
	{
		AlphaTest = 1 << 0,
		Bloom = 1 << 1,
		DynamicLights = 1 << 2,
		SoftSprite = 1 << 3,
		SunLight = 1 << 4,
		Num = 1 << 5
	};
};

struct DepthFragmentShaderVariant
{
	enum
	{
		AlphaTest = 1 << 0,
		Num = 1 << 1
	};
};

struct DepthVertexShaderVariant
{
	enum
	{
		AlphaTest = 1 << 0,
		Num = 1 << 1
	};
};

struct FogFragmentShaderVariant
{
	enum
	{
		Bloom = 1 << 0,
		Num = 1 << 1
	};
};

struct TextureVariationFragmentShaderVariant
{
	enum
	{
		Bloom = 1 << 0,
		SunLight = 1 << 1,
		Num = 1 << 2
	};
};

