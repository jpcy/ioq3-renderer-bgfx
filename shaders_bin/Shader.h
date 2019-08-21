struct FragmentShaderId
{
	enum Enum
	{
		Bloom,
		Color,
		Depth,
		Depth_AlphaTest,
		Fog,
		GaussianBlur,
		Generic,
		Generic_AlphaTest,
		Generic_DynamicLights,
		Generic_AlphaTestDynamicLights,
		Generic_SoftSprite,
		Generic_AlphaTestSoftSprite,
		Generic_DynamicLightsSoftSprite,
		Generic_AlphaTestDynamicLightsSoftSprite,
		Generic_SunLight,
		Generic_AlphaTestSunLight,
		Generic_DynamicLightsSunLight,
		Generic_AlphaTestDynamicLightsSunLight,
		Generic_SoftSpriteSunLight,
		Generic_AlphaTestSoftSpriteSunLight,
		Generic_DynamicLightsSoftSpriteSunLight,
		Generic_AlphaTestDynamicLightsSoftSpriteSunLight,
		HemicubeDownsample,
		HemicubeWeightedDownsample,
		SMAABlendingWeightCalculation,
		SMAAEdgeDetection,
		SMAANeighborhoodBlending,
		Texture,
		TextureColor,
		TextureDebug,
		TextureVariation,
		TextureVariation_SunLight,
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
	"GaussianBlur",
	"Generic",
	"Generic_AlphaTest",
	"Generic_DynamicLights",
	"Generic_AlphaTestDynamicLights",
	"Generic_SoftSprite",
	"Generic_AlphaTestSoftSprite",
	"Generic_DynamicLightsSoftSprite",
	"Generic_AlphaTestDynamicLightsSoftSprite",
	"Generic_SunLight",
	"Generic_AlphaTestSunLight",
	"Generic_DynamicLightsSunLight",
	"Generic_AlphaTestDynamicLightsSunLight",
	"Generic_SoftSpriteSunLight",
	"Generic_AlphaTestSoftSpriteSunLight",
	"Generic_DynamicLightsSoftSpriteSunLight",
	"Generic_AlphaTestDynamicLightsSoftSpriteSunLight",
	"HemicubeDownsample",
	"HemicubeWeightedDownsample",
	"SMAABlendingWeightCalculation",
	"SMAAEdgeDetection",
	"SMAANeighborhoodBlending",
	"Texture",
	"TextureColor",
	"TextureDebug",
	"TextureVariation",
	"TextureVariation_SunLight",
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
		DynamicLights = 1 << 1,
		SoftSprite = 1 << 2,
		SunLight = 1 << 3,
		Num = 1 << 4
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

struct TextureVariationFragmentShaderVariant
{
	enum
	{
		SunLight = 1 << 0,
		Num = 1 << 1
	};
};

