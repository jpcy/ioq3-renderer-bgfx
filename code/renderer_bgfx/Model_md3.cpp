/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "Precompiled.h"
#pragma hdrstop

namespace renderer {

/*
========================================================================

.MD3 triangle model file format

========================================================================
*/

#define MD3_IDENT			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

	// limits
#define MD3_MAX_LODS		3
#define	MD3_MAX_TRIANGLES	8192	// per surface
#define MD3_MAX_VERTS		4096	// per surface
#define MD3_MAX_SHADERS		256		// per surface
#define MD3_MAX_FRAMES		1024	// per model
#define	MD3_MAX_SURFACES	32		// per model
#define MD3_MAX_TAGS		16		// per frame

	// vertex scales
#define	MD3_XYZ_SCALE		(1.0f/64)

typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

typedef struct md3Tag_s {
	char		name[MAX_QPATH];	// tag name
	vec3_t		origin;
	vec3_t		axis[3];
} md3Tag_t;

/*
** md3Surface_t
**
** CHUNK			SIZE
** header			sizeof( md3Surface_t )
** shaders			sizeof( md3Shader_t ) * numShaders
** triangles[0]		sizeof( md3Triangle_t ) * numTriangles
** st				sizeof( md3St_t ) * numVerts
** XyzNormals		sizeof( md3XyzNormal_t ) * numVerts * numFrames
*/
typedef struct {
	int		ident;				// 

	char	name[MAX_QPATH];	// polyset name

	int		flags;
	int		numFrames;			// all surfaces in a model should have the same

	int		numShaders;			// all surfaces in a model should have the same
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			// offset from start of md3Surface_t
	int		ofsSt;				// texture coords are common for all frames
	int		ofsXyzNormals;		// numVerts * numFrames

	int		ofsEnd;				// next surface follows
} md3Surface_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;	// for in-game use
} md3Shader_t;

typedef struct {
	int			indexes[3];
} md3Triangle_t;

typedef struct {
	float		st[2];
} md3St_t;

typedef struct {
	short		xyz[3];
	short		normal;
} md3XyzNormal_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	// model name

	int			flags;

	int			numFrames;
	int			numTags;
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;			// offset for first frame
	int			ofsTags;			// numFrames * numTags
	int			ofsSurfaces;		// first surface, others follow

	int			ofsEnd;				// end of file
} md3Header_t;

// Ridah, mesh compression
/*
==============================================================================

MDC file format

==============================================================================
*/

#define MDC_IDENT           ( ( 'C' << 24 ) + ( 'P' << 16 ) + ( 'D' << 8 ) + 'I' )
#define MDC_VERSION         2

// version history:
// 1 - original
// 2 - changed tag structure so it only lists the names once

typedef struct {
	unsigned int ofsVec;                    // offset direction from the last base frame
											//	unsigned short	ofsVec;
} mdcXyzCompressed_t;

typedef struct {
	char name[MAX_QPATH];           // tag name
} mdcTagName_t;

#define MDC_TAG_ANGLE_SCALE ( 360.0f / 32700.0f )

typedef struct {
	short xyz[3];
	short angles[3];
} mdcTag_t;

/*
** mdcSurface_t
**
** CHUNK			SIZE
** header			sizeof( md3Surface_t )
** shaders			sizeof( md3Shader_t ) * numShaders
** triangles[0]		sizeof( md3Triangle_t ) * numTriangles
** st				sizeof( md3St_t ) * numVerts
** XyzNormals		sizeof( md3XyzNormal_t ) * numVerts * numBaseFrames
** XyzCompressed	sizeof( mdcXyzCompressed ) * numVerts * numCompFrames
** frameBaseFrames	sizeof( short ) * numFrames
** frameCompFrames	sizeof( short ) * numFrames (-1 if frame is a baseFrame)
*/
typedef struct {
	int ident;                  //

	char name[MAX_QPATH];       // polyset name

	int flags;
	int numCompFrames;          // all surfaces in a model should have the same
	int numBaseFrames;          // ditto

	int numShaders;             // all surfaces in a model should have the same
	int numVerts;

	int numTriangles;
	int ofsTriangles;

	int ofsShaders;             // offset from start of md3Surface_t
	int ofsSt;                  // texture coords are common for all frames
	int ofsXyzNormals;          // numVerts * numBaseFrames
	int ofsXyzCompressed;       // numVerts * numCompFrames

	int ofsFrameBaseFrames;     // numFrames
	int ofsFrameCompFrames;     // numFrames

	int ofsEnd;                 // next surface follows
} mdcSurface_t;

typedef struct {
	int ident;
	int version;

	char name[MAX_QPATH];           // model name

	int flags;

	int numFrames;
	int numTags;
	int numSurfaces;

	int numSkins;

	int ofsFrames;                  // offset for first frame, stores the bounds and localOrigin
	int ofsTagNames;                // numTags
	int ofsTags;                    // numFrames * numTags
	int ofsSurfaces;                // first surface, others follow

	int ofsEnd;                     // end of file
} mdcHeader_t;
// done.

// Ridah, mesh compression
#define NUMMDCVERTEXNORMALS  256

static float s_anormals[NUMMDCVERTEXNORMALS][3] =
{
	{ 1.0f, 0.0f, 0.0f },{ 0.980785f, 0.19509f, 0.0f },{ 0.92388f, 0.382683f, 0.0f },{ 0.83147f, 0.55557f, 0.0f },
	{ 0.707107f, 0.707107f, 0.0f },{ 0.55557f, 0.83147f, 0.0f },{ 0.382683f, 0.92388f, 0.0f },{ 0.19509f, 0.980785f, 0.0f },
	{ -0.0f, 1.0f, 0.0f },{ -0.19509f, 0.980785f, 0.0f },{ -0.382683f, 0.92388f, 0.0f },{ -0.55557f, 0.83147f, 0.0f },
	{ -0.707107f, 0.707107f, 0.0f },{ -0.83147f, 0.55557f, 0.0f },{ -0.92388f, 0.382683f, 0.0f },{ -0.980785f, 0.19509f, 0.0f },
	{ -1.0f, -0.0f, 0.0f },{ -0.980785f, -0.19509f, 0.0f },{ -0.92388f, -0.382683f, 0.0f },{ -0.83147f, -0.55557f, 0.0f },
	{ -0.707107f, -0.707107f, 0.0f },{ -0.55557f, -0.831469f, 0.0f },{ -0.382684f, -0.92388f, 0.0f },{ -0.19509f, -0.980785f, 0.0f },
	{ 0.0f, -1.0f, 0.0f },{ 0.19509f, -0.980785f, 0.0f },{ 0.382684f, -0.923879f, 0.0f },{ 0.55557f, -0.83147f, 0.0f },
	{ 0.707107f, -0.707107f, 0.0f },{ 0.83147f, -0.55557f, 0.0f },{ 0.92388f, -0.382683f, 0.0f },{ 0.980785f, -0.19509f, 0.0f },
	{ 0.980785f, 0.0f, -0.19509f },{ 0.956195f, 0.218245f, -0.19509f },{ 0.883657f, 0.425547f, -0.19509f },{ 0.766809f, 0.61151f, -0.19509f },
	{ 0.61151f, 0.766809f, -0.19509f },{ 0.425547f, 0.883657f, -0.19509f },{ 0.218245f, 0.956195f, -0.19509f },{ -0.0f, 0.980785f, -0.19509f },
	{ -0.218245f, 0.956195f, -0.19509f },{ -0.425547f, 0.883657f, -0.19509f },{ -0.61151f, 0.766809f, -0.19509f },{ -0.766809f, 0.61151f, -0.19509f },
	{ -0.883657f, 0.425547f, -0.19509f },{ -0.956195f, 0.218245f, -0.19509f },{ -0.980785f, -0.0f, -0.19509f },{ -0.956195f, -0.218245f, -0.19509f },
	{ -0.883657f, -0.425547f, -0.19509f },{ -0.766809f, -0.61151f, -0.19509f },{ -0.61151f, -0.766809f, -0.19509f },{ -0.425547f, -0.883657f, -0.19509f },
	{ -0.218245f, -0.956195f, -0.19509f },{ 0.0f, -0.980785f, -0.19509f },{ 0.218245f, -0.956195f, -0.19509f },{ 0.425547f, -0.883657f, -0.19509f },
	{ 0.61151f, -0.766809f, -0.19509f },{ 0.766809f, -0.61151f, -0.19509f },{ 0.883657f, -0.425547f, -0.19509f },{ 0.956195f, -0.218245f, -0.19509f },
	{ 0.92388f, 0.0f, -0.382683f },{ 0.892399f, 0.239118f, -0.382683f },{ 0.800103f, 0.46194f, -0.382683f },{ 0.653281f, 0.653281f, -0.382683f },
	{ 0.46194f, 0.800103f, -0.382683f },{ 0.239118f, 0.892399f, -0.382683f },{ -0.0f, 0.92388f, -0.382683f },{ -0.239118f, 0.892399f, -0.382683f },
	{ -0.46194f, 0.800103f, -0.382683f },{ -0.653281f, 0.653281f, -0.382683f },{ -0.800103f, 0.46194f, -0.382683f },{ -0.892399f, 0.239118f, -0.382683f },
	{ -0.92388f, -0.0f, -0.382683f },{ -0.892399f, -0.239118f, -0.382683f },{ -0.800103f, -0.46194f, -0.382683f },{ -0.653282f, -0.653281f, -0.382683f },
	{ -0.46194f, -0.800103f, -0.382683f },{ -0.239118f, -0.892399f, -0.382683f },{ 0.0f, -0.92388f, -0.382683f },{ 0.239118f, -0.892399f, -0.382683f },
	{ 0.46194f, -0.800103f, -0.382683f },{ 0.653281f, -0.653282f, -0.382683f },{ 0.800103f, -0.46194f, -0.382683f },{ 0.892399f, -0.239117f, -0.382683f },
	{ 0.83147f, 0.0f, -0.55557f },{ 0.790775f, 0.256938f, -0.55557f },{ 0.672673f, 0.488726f, -0.55557f },{ 0.488726f, 0.672673f, -0.55557f },
	{ 0.256938f, 0.790775f, -0.55557f },{ -0.0f, 0.83147f, -0.55557f },{ -0.256938f, 0.790775f, -0.55557f },{ -0.488726f, 0.672673f, -0.55557f },
	{ -0.672673f, 0.488726f, -0.55557f },{ -0.790775f, 0.256938f, -0.55557f },{ -0.83147f, -0.0f, -0.55557f },{ -0.790775f, -0.256938f, -0.55557f },
	{ -0.672673f, -0.488726f, -0.55557f },{ -0.488725f, -0.672673f, -0.55557f },{ -0.256938f, -0.790775f, -0.55557f },{ 0.0f, -0.83147f, -0.55557f },
	{ 0.256938f, -0.790775f, -0.55557f },{ 0.488725f, -0.672673f, -0.55557f },{ 0.672673f, -0.488726f, -0.55557f },{ 0.790775f, -0.256938f, -0.55557f },
	{ 0.707107f, 0.0f, -0.707107f },{ 0.653281f, 0.270598f, -0.707107f },{ 0.5f, 0.5f, -0.707107f },{ 0.270598f, 0.653281f, -0.707107f },
	{ -0.0f, 0.707107f, -0.707107f },{ -0.270598f, 0.653282f, -0.707107f },{ -0.5f, 0.5f, -0.707107f },{ -0.653281f, 0.270598f, -0.707107f },
	{ -0.707107f, -0.0f, -0.707107f },{ -0.653281f, -0.270598f, -0.707107f },{ -0.5f, -0.5f, -0.707107f },{ -0.270598f, -0.653281f, -0.707107f },
	{ 0.0f, -0.707107f, -0.707107f },{ 0.270598f, -0.653281f, -0.707107f },{ 0.5f, -0.5f, -0.707107f },{ 0.653282f, -0.270598f, -0.707107f },
	{ 0.55557f, 0.0f, -0.83147f },{ 0.481138f, 0.277785f, -0.83147f },{ 0.277785f, 0.481138f, -0.83147f },{ -0.0f, 0.55557f, -0.83147f },
	{ -0.277785f, 0.481138f, -0.83147f },{ -0.481138f, 0.277785f, -0.83147f },{ -0.55557f, -0.0f, -0.83147f },{ -0.481138f, -0.277785f, -0.83147f },
	{ -0.277785f, -0.481138f, -0.83147f },{ 0.0f, -0.55557f, -0.83147f },{ 0.277785f, -0.481138f, -0.83147f },{ 0.481138f, -0.277785f, -0.83147f },
	{ 0.382683f, 0.0f, -0.92388f },{ 0.270598f, 0.270598f, -0.92388f },{ -0.0f, 0.382683f, -0.92388f },{ -0.270598f, 0.270598f, -0.92388f },
	{ -0.382683f, -0.0f, -0.92388f },{ -0.270598f, -0.270598f, -0.92388f },{ 0.0f, -0.382683f, -0.92388f },{ 0.270598f, -0.270598f, -0.92388f },
	{ 0.19509f, 0.0f, -0.980785f },{ -0.0f, 0.19509f, -0.980785f },{ -0.19509f, -0.0f, -0.980785f },{ 0.0f, -0.19509f, -0.980785f },
	{ 0.980785f, 0.0f, 0.19509f },{ 0.956195f, 0.218245f, 0.19509f },{ 0.883657f, 0.425547f, 0.19509f },{ 0.766809f, 0.61151f, 0.19509f },
	{ 0.61151f, 0.766809f, 0.19509f },{ 0.425547f, 0.883657f, 0.19509f },{ 0.218245f, 0.956195f, 0.19509f },{ -0.0f, 0.980785f, 0.19509f },
	{ -0.218245f, 0.956195f, 0.19509f },{ -0.425547f, 0.883657f, 0.19509f },{ -0.61151f, 0.766809f, 0.19509f },{ -0.766809f, 0.61151f, 0.19509f },
	{ -0.883657f, 0.425547f, 0.19509f },{ -0.956195f, 0.218245f, 0.19509f },{ -0.980785f, -0.0f, 0.19509f },{ -0.956195f, -0.218245f, 0.19509f },
	{ -0.883657f, -0.425547f, 0.19509f },{ -0.766809f, -0.61151f, 0.19509f },{ -0.61151f, -0.766809f, 0.19509f },{ -0.425547f, -0.883657f, 0.19509f },
	{ -0.218245f, -0.956195f, 0.19509f },{ 0.0f, -0.980785f, 0.19509f },{ 0.218245f, -0.956195f, 0.19509f },{ 0.425547f, -0.883657f, 0.19509f },
	{ 0.61151f, -0.766809f, 0.19509f },{ 0.766809f, -0.61151f, 0.19509f },{ 0.883657f, -0.425547f, 0.19509f },{ 0.956195f, -0.218245f, 0.19509f },
	{ 0.92388f, 0.0f, 0.382683f },{ 0.892399f, 0.239118f, 0.382683f },{ 0.800103f, 0.46194f, 0.382683f },{ 0.653281f, 0.653281f, 0.382683f },
	{ 0.46194f, 0.800103f, 0.382683f },{ 0.239118f, 0.892399f, 0.382683f },{ -0.0f, 0.92388f, 0.382683f },{ -0.239118f, 0.892399f, 0.382683f },
	{ -0.46194f, 0.800103f, 0.382683f },{ -0.653281f, 0.653281f, 0.382683f },{ -0.800103f, 0.46194f, 0.382683f },{ -0.892399f, 0.239118f, 0.382683f },
	{ -0.92388f, -0.0f, 0.382683f },{ -0.892399f, -0.239118f, 0.382683f },{ -0.800103f, -0.46194f, 0.382683f },{ -0.653282f, -0.653281f, 0.382683f },
	{ -0.46194f, -0.800103f, 0.382683f },{ -0.239118f, -0.892399f, 0.382683f },{ 0.0f, -0.92388f, 0.382683f },{ 0.239118f, -0.892399f, 0.382683f },
	{ 0.46194f, -0.800103f, 0.382683f },{ 0.653281f, -0.653282f, 0.382683f },{ 0.800103f, -0.46194f, 0.382683f },{ 0.892399f, -0.239117f, 0.382683f },
	{ 0.83147f, 0.0f, 0.55557f },{ 0.790775f, 0.256938f, 0.55557f },{ 0.672673f, 0.488726f, 0.55557f },{ 0.488726f, 0.672673f, 0.55557f },
	{ 0.256938f, 0.790775f, 0.55557f },{ -0.0f, 0.83147f, 0.55557f },{ -0.256938f, 0.790775f, 0.55557f },{ -0.488726f, 0.672673f, 0.55557f },
	{ -0.672673f, 0.488726f, 0.55557f },{ -0.790775f, 0.256938f, 0.55557f },{ -0.83147f, -0.0f, 0.55557f },{ -0.790775f, -0.256938f, 0.55557f },
	{ -0.672673f, -0.488726f, 0.55557f },{ -0.488725f, -0.672673f, 0.55557f },{ -0.256938f, -0.790775f, 0.55557f },{ 0.0f, -0.83147f, 0.55557f },
	{ 0.256938f, -0.790775f, 0.55557f },{ 0.488725f, -0.672673f, 0.55557f },{ 0.672673f, -0.488726f, 0.55557f },{ 0.790775f, -0.256938f, 0.55557f },
	{ 0.707107f, 0.0f, 0.707107f },{ 0.653281f, 0.270598f, 0.707107f },{ 0.5f, 0.5f, 0.707107f },{ 0.270598f, 0.653281f, 0.707107f },
	{ -0.0f, 0.707107f, 0.707107f },{ -0.270598f, 0.653282f, 0.707107f },{ -0.5f, 0.5f, 0.707107f },{ -0.653281f, 0.270598f, 0.707107f },
	{ -0.707107f, -0.0f, 0.707107f },{ -0.653281f, -0.270598f, 0.707107f },{ -0.5f, -0.5f, 0.707107f },{ -0.270598f, -0.653281f, 0.707107f },
	{ 0.0f, -0.707107f, 0.707107f },{ 0.270598f, -0.653281f, 0.707107f },{ 0.5f, -0.5f, 0.707107f },{ 0.653282f, -0.270598f, 0.707107f },
	{ 0.55557f, 0.0f, 0.83147f },{ 0.481138f, 0.277785f, 0.83147f },{ 0.277785f, 0.481138f, 0.83147f },{ -0.0f, 0.55557f, 0.83147f },
	{ -0.277785f, 0.481138f, 0.83147f },{ -0.481138f, 0.277785f, 0.83147f },{ -0.55557f, -0.0f, 0.83147f },{ -0.481138f, -0.277785f, 0.83147f },
	{ -0.277785f, -0.481138f, 0.83147f },{ 0.0f, -0.55557f, 0.83147f },{ 0.277785f, -0.481138f, 0.83147f },{ 0.481138f, -0.277785f, 0.83147f },
	{ 0.382683f, 0.0f, 0.92388f },{ 0.270598f, 0.270598f, 0.92388f },{ -0.0f, 0.382683f, 0.92388f },{ -0.270598f, 0.270598f, 0.92388f },
	{ -0.382683f, -0.0f, 0.92388f },{ -0.270598f, -0.270598f, 0.92388f },{ 0.0f, -0.382683f, 0.92388f },{ 0.270598f, -0.270598f, 0.92388f },
	{ 0.19509f, 0.0f, 0.980785f },{ -0.0f, 0.19509f, 0.980785f },{ -0.19509f, -0.0f, 0.980785f },{ 0.0f, -0.19509f, 0.980785f }
};

// NOTE: MDC_MAX_ERROR is effectively the compression level. the lower this value, the higher
// the accuracy, but with lower compression ratios.
#define MDC_MAX_ERROR       0.1f     // if any compressed vert is off by more than this from the
// actual vert, make this a baseframe

#define MDC_DIST_SCALE      0.05f    // lower for more accuracy, but less range

// note: we are locked in at 8 or less bits since changing to byte-encoded normals
#define MDC_BITS_PER_AXIS   8
#define MDC_MAX_OFS         127.0f   // to be safe

#define MDC_MAX_DIST        ( MDC_MAX_OFS * MDC_DIST_SCALE )

class Model_md3 : public Model
{
public:
	Model_md3(const char *name, bool compressed);
	bool load(const ReadOnlyFile &file) override;
	Bounds getBounds() const override;
	Material *getMaterial(size_t surfaceNo) const override { return nullptr; }
	bool isCulled(Entity *entity, const Frustum &cameraFrustum) const override;
	int lerpTag(const char *name, const Entity &entity, int startIndex, Transform *transform) const override;
	void render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity) override;

private:
	struct Frame
	{
		Bounds bounds;
		vec3 position;
		float radius;
		std::vector<Transform> tags;

		/// Vertex data in system memory. Used by animated models.
		std::vector<Vertex> vertices;
	};

	struct Surface
	{
		char name[MAX_QPATH]; // polyset name
		std::vector<Material *> materials;
		uint32_t startIndex;
		uint32_t nIndices;
		uint32_t startVertex;
		uint32_t nVertices;
	};

	struct TagName
	{
		char name[MAX_QPATH];
	};

	vec3 decodeNormal(short normal) const;
	int getTag(const char *name, int frame, int startIndex, Transform *transform) const;

	bool compressed_;

	IndexBuffer indexBuffer_;

	/// Need to keep a copy of the model indices in system memory for CPU deforms.
	std::vector<uint16_t> indices_;

	/// Static model vertex buffer.
	VertexBuffer vertexBuffer_;

	/// The number of vertices in all the surfaces of a single frame.
	uint32_t nVertices_;

	std::vector<Frame> frames_;
	std::vector<TagName> tagNames_;
	std::vector<Surface> surfaces_;
};

std::unique_ptr<Model> Model::createMD3(const char *name)
{
	return std::make_unique<Model_md3>(name, false);
}

std::unique_ptr<Model> Model::createMDC(const char *name)
{
	return std::make_unique<Model_md3>(name, true);
}

Model_md3::Model_md3(const char *name, bool compressed)
{
	util::Strncpyz(name_, name, sizeof(name_));
	compressed_ = compressed;
}

struct FileHeader
{
	int ident;
	int version;
	int nFrames;
	int nTags;
	int nSurfaces;
	int nSkins;
	int framesOffset;
	int tagNamesOffset; // compressed only
	int tagsOffset;
	int surfacesOffset;
};

#define COPY_HEADER \
header.ident = fileHeader->ident; \
header.version = fileHeader->version; \
header.nFrames = fileHeader->numFrames; \
header.nTags = fileHeader->numTags; \
header.nSurfaces = fileHeader->numSurfaces; \
header.nSkins = fileHeader->numSkins; \
header.framesOffset = fileHeader->ofsFrames; \
header.tagsOffset = fileHeader->ofsTags; \
header.surfacesOffset = fileHeader->ofsSurfaces;

struct FileSurface
{
	uint8_t *offset;
	char name[MAX_QPATH];
	int nCompressedFrames; // compressed only
	int nBaseFrames; // compressed only
	int nShaders;
	int nVertices;
	int nTriangles;
	int trianglesOffset;
	int shadersOffset;
	int uvsOffset;
	int positionNormalOffset;
	int positionNormalCompressedOffset; // compressed only
	int baseFramesOffset; // compressed only
	int compressedFramesOffset; // compressed only
};

bool Model_md3::load(const ReadOnlyFile &file)
{
	const uint8_t *data = file.getData();

	// Header
	FileHeader header;

	if (compressed_)
	{
		auto fileHeader = (mdcHeader_t *)data;
		COPY_HEADER
		header.tagNamesOffset = fileHeader->ofsTagNames;
	}
	else
	{
		auto fileHeader = (md3Header_t *)data;
		COPY_HEADER
	}
	
	const int validIdent = compressed_ ? MDC_IDENT : MD3_IDENT;
	const int validVersion = compressed_ ? MDC_VERSION : MD3_VERSION;

	if (header.ident != validIdent)
	{
		interface::PrintWarningf("Model %s: wrong ident (%i should be %i)\n", name_, header.ident, validIdent);
		return false;
	}

	if (header.version != validVersion)
	{
		interface::PrintWarningf("Model %s: wrong version (%i should be %i)\n", name_, header.version, validVersion);
		return false;
	}

	if (header.nFrames < 1)
	{
		interface::PrintWarningf("Model %s: no frames\n", name_);
		return false;
	}

	// Frames
	auto fileFrames = (const md3Frame_t *)&data[header.framesOffset];
	frames_.resize(header.nFrames);

	for (int i = 0; i < header.nFrames; i++)
	{
		Frame &frame = frames_[i];
		const md3Frame_t &fileFrame = fileFrames[i];
		frame.radius = fileFrame.radius;
		frame.bounds = Bounds(fileFrame.bounds[0], fileFrame.bounds[1]);
		frame.position = fileFrame.localOrigin;

		if (compressed_ && (strstr(name_, "sherman") || strstr(name_, "mg42")))
		{
			frame.radius = 256;
			frame.bounds = Bounds(vec3(128, 128, 128), vec3(-128, -128, -128));
		}

		// Tags
		frame.tags.resize(header.nTags);

		for (int j = 0; j < header.nTags; j++)
		{
			Transform &tag = frame.tags[j];

			if (compressed_)
			{
				const auto &fileTag = ((const mdcTag_t *)&data[header.tagsOffset])[j + i * header.nTags];
				vec3 angles;

				for (int k = 0; k < 3; k++)
				{
					tag.position[k] = (float)fileTag.xyz[k] * MD3_XYZ_SCALE;
					angles[k] = (float)fileTag.angles[k] * MDC_TAG_ANGLE_SCALE;
				}

				tag.rotation = mat3(angles);
			}
			else
			{
				const auto &fileTag = ((const md3Tag_t *)&data[header.tagsOffset])[j + i * header.nTags];
				tag.position = fileTag.origin;
				tag.rotation[0] = fileTag.axis[0];
				tag.rotation[1] = fileTag.axis[1];
				tag.rotation[2] = fileTag.axis[2];
			}
		}
	}

	// Tag names
	tagNames_.resize(header.nTags);

	if (compressed_)
	{
		auto fileTagNames = (const mdcTagName_t *)&data[header.tagNamesOffset];

		for (int i = 0; i < header.nTags; i++)
		{
			util::Strncpyz(tagNames_[i].name, fileTagNames[i].name, sizeof(tagNames_[i].name));
		}
	}
	else
	{
		auto fileTags = (const md3Tag_t *)&data[header.tagsOffset];

		for (int i = 0; i < header.nTags; i++)
		{
			util::Strncpyz(tagNames_[i].name, fileTags[i].name, sizeof(tagNames_[i].name));
		}
	}

	// Copy uncompressed and compression surface data into a common struct.
	std::vector<FileSurface> fileSurfaces(header.nSurfaces);

	if (compressed_)
	{
		auto mdcSurface = (mdcSurface_t *)&data[header.surfacesOffset];

		for (int i = 0; i < header.nSurfaces; i++)
		{
			FileSurface &fs = fileSurfaces[i];
			fs.offset = (uint8_t *)mdcSurface;
			util::Strncpyz(fs.name, mdcSurface->name, sizeof(fs.name));
			fs.nCompressedFrames = mdcSurface->numCompFrames;
			fs.nBaseFrames = mdcSurface->numBaseFrames;
			fs.nShaders = mdcSurface->numShaders;
			fs.nVertices = mdcSurface->numVerts;
			fs.nTriangles = mdcSurface->numTriangles;
			fs.trianglesOffset = mdcSurface->ofsTriangles;
			fs.shadersOffset = mdcSurface->ofsShaders;
			fs.uvsOffset = mdcSurface->ofsSt;
			fs.positionNormalOffset = mdcSurface->ofsXyzNormals;
			fs.positionNormalCompressedOffset = mdcSurface->ofsXyzCompressed;
			fs.baseFramesOffset = mdcSurface->ofsFrameBaseFrames;
			fs.compressedFramesOffset = mdcSurface->ofsFrameCompFrames;

			// Move to the next surface.
			mdcSurface = (mdcSurface_t *)((uint8_t *)mdcSurface + mdcSurface->ofsEnd);
		}
	}
	else
	{
		auto md3Surface = (md3Surface_t *)&data[header.surfacesOffset];

		for (int i = 0; i < header.nSurfaces; i++)
		{
			FileSurface &fs = fileSurfaces[i];
			fs.offset = (uint8_t *)md3Surface;
			util::Strncpyz(fs.name, md3Surface->name, sizeof(fs.name));
			fs.nShaders = md3Surface->numShaders;
			fs.nVertices = md3Surface->numVerts;
			fs.nTriangles = md3Surface->numTriangles;
			fs.trianglesOffset = md3Surface->ofsTriangles;
			fs.shadersOffset = md3Surface->ofsShaders;
			fs.uvsOffset = md3Surface->ofsSt;
			fs.positionNormalOffset = md3Surface->ofsXyzNormals;

			// Move to the next surface.
			md3Surface = (md3Surface_t *)((uint8_t *)md3Surface + md3Surface->ofsEnd);
		}
	}

	// Surfaces
	surfaces_.resize(header.nSurfaces);
	size_t nIndices = 0;
	nVertices_ = 0;

	for (int i = 0; i < header.nSurfaces; i++)
	{
		FileSurface &fs = fileSurfaces[i];
		Surface &s = surfaces_[i];
		util::Strncpyz(s.name, fs.name, sizeof(s.name));
		util::ToLowerCase(s.name); // Lowercase the surface name so skin compares are faster.

		// Strip off a trailing _1 or _2. This is a crutch for q3data being a mess.
		size_t n = strlen(s.name);

		if (n > 2 && s.name[n - 2] == '_')
		{
			s.name[n - 2] = 0;
		}

		// Surface materials
		auto fileShaders = (md3Shader_t *)(fs.offset + fs.shadersOffset);
		s.materials.resize(fs.nShaders);

		for (int j = 0; j < fs.nShaders; j++)
		{
			s.materials[j] = g_materialCache->findMaterial(fileShaders[j].name, MaterialLightmapId::None);
		}

		// Total the number of indices and vertices in each surface.
		nIndices += fs.nTriangles * 3;
		nVertices_ += fs.nVertices;
	}

	// Stop here if the model doesn't have any geometry (e.g. weapon hand models).
	if (nIndices == 0)
		return true;

	const bool isAnimated = frames_.size() > 1;

	// Merge all surface indices into one index buffer. For each surface, store the start index and number of indices.
	const bgfx::Memory *indicesMem = bgfx::alloc(uint32_t(sizeof(uint16_t) * nIndices));
	auto indices = (uint16_t *)indicesMem->data;
	uint32_t startIndex = 0, startVertex = 0;

	for (int i = 0; i < header.nSurfaces; i++)
	{
		FileSurface &fs = fileSurfaces[i];
		Surface &surface = surfaces_[i];
		surface.startIndex = startIndex;
		surface.nIndices = fs.nTriangles * 3;
		auto fileIndices = (int *)(fs.offset + fs.trianglesOffset);

		for (uint32_t j = 0; j < surface.nIndices; j++)
		{
			indices[startIndex + j] = startVertex + fileIndices[j];
		}

		startIndex += surface.nIndices;
		startVertex += fs.nVertices;
	}

	indexBuffer_.handle = bgfx::createIndexBuffer(indicesMem);

	// Keep a copy of indices in system memory for CPU deforms.
	if (isAnimated)
	{
		indices_.resize(nIndices);
		memcpy(indices_.data(), indices, sizeof(uint16_t) * nIndices);
	}

	// Vertices
	// Texture coords are the same for each frame, positions and normals aren't.
	// Static models (models with 1 frame) have their surface vertices merged into a single vertex buffer.
	// Animated models (models with more than 1 frame) have their surface vertices merged into a single system memory vertex array for each frame.
	if (!isAnimated)
	{
		const bgfx::Memory *verticesMem = bgfx::alloc(sizeof(Vertex) * nVertices_);
		auto vertices = (Vertex *)verticesMem->data;
		frames_[0].vertices.resize(nVertices_);
		size_t startVertex = 0;

		for (int i = 0; i < header.nSurfaces; i++)
		{
			FileSurface &fs = fileSurfaces[i];
			Surface &surface = surfaces_[i];
			auto fileTexCoords = (md3St_t *)(fs.offset + fs.uvsOffset);
			auto fileXyzNormals = (md3XyzNormal_t *)(fs.offset + fs.positionNormalOffset);

			for (int j = 0; j < fs.nVertices; j++)
			{
				Vertex &v = vertices[startVertex + j];
				v.pos.x = fileXyzNormals[j].xyz[0] * MD3_XYZ_SCALE;
				v.pos.y = fileXyzNormals[j].xyz[1] * MD3_XYZ_SCALE;
				v.pos.z = fileXyzNormals[j].xyz[2] * MD3_XYZ_SCALE;
				v.setNormal(decodeNormal(fileXyzNormals[j].normal));
				v.setTexCoord(fileTexCoords[j].st[0], fileTexCoords[j].st[1]);
				v.setColor(vec4::white);
			}

			startVertex += fs.nVertices;
		}

		vertexBuffer_.handle = bgfx::createVertexBuffer(verticesMem, Vertex::decl);
	}
	else
	{
		for (int i = 0; i < header.nFrames; i++)
		{
			frames_[i].vertices.resize(nVertices_);
		}

		uint32_t startVertex = 0;

		for (int i = 0; i < header.nSurfaces; i++)
		{
			FileSurface &fs = fileSurfaces[i];
			Surface &surface = surfaces_[i];
			auto fileBaseFrames = (short *)(fs.offset + fs.baseFramesOffset);
			auto fileCompressedFrames = (short *)(fs.offset + fs.compressedFramesOffset);

			// Texture coords are the same for each frame, positions and normals aren't.
			auto fileTexCoords = (md3St_t *)(fs.offset + fs.uvsOffset);

			for (int j = 0; j < header.nFrames; j++)
			{
				int positionNormalFrame;

				if (compressed_)
				{
					positionNormalFrame = fileBaseFrames[j];
				}
				else
				{
					positionNormalFrame = j;
				}

				auto fileXyzNormals = (md3XyzNormal_t *)(fs.offset + fs.positionNormalOffset + positionNormalFrame * sizeof(md3XyzNormal_t) * fs.nVertices);

				for (int k = 0; k < fs.nVertices; k++)
				{
					Vertex &v = frames_[j].vertices[startVertex + k];
					v.pos.x = fileXyzNormals[k].xyz[0] * MD3_XYZ_SCALE;
					v.pos.y = fileXyzNormals[k].xyz[1] * MD3_XYZ_SCALE;
					v.pos.z = fileXyzNormals[k].xyz[2] * MD3_XYZ_SCALE;
					v.setNormal(decodeNormal(fileXyzNormals[k].normal));
					v.setTexCoord(fileTexCoords[k].st[0], fileTexCoords[k].st[1]);
					v.setColor(vec4::white);

					if (compressed_)
					{
						// If compressedFrameIndex isn't -1, use compressedFrameIndex as a delta from baseFrameIndex.
						short compressedFrameIndex = fileCompressedFrames[j];

						if (compressedFrameIndex != -1)
						{
							auto fileXyzCompressed = (mdcXyzCompressed_t *)(fs.offset + fs.positionNormalCompressedOffset + compressedFrameIndex * sizeof(mdcXyzCompressed_t) * fs.nVertices);
							vec3 delta;
							delta[0] = (float((fileXyzCompressed[k].ofsVec) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;
							delta[1] = (float((fileXyzCompressed[k].ofsVec >> 8) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;
							delta[2] = (float((fileXyzCompressed[k].ofsVec >> 16) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;
							v.pos += delta;
							v.setNormal(vec3(s_anormals[fileXyzCompressed[k].ofsVec >> 24]));
						}
					}
				}
			}

			surface.startVertex = startVertex;
			surface.nVertices = fs.nVertices;
			startVertex += fs.nVertices;
		}
	}

	return true;
}

Bounds Model_md3::getBounds() const
{
	return frames_[0].bounds;
}

bool Model_md3::isCulled(Entity *entity, const Frustum &cameraFrustum) const
{
	assert(entity);

	// It is possible to have a bad frame while changing models.
	const int frameIndex = Clamped(entity->frame, 0, (int)frames_.size() - 1);
	const int oldFrameIndex = Clamped(entity->oldFrame, 0, (int)frames_.size() - 1);
	const Frame &frame = frames_[frameIndex];
	const Frame &oldFrame = frames_[oldFrameIndex];
	const mat4 modelMatrix = mat4::transform(entity->rotation, entity->position);

	// Cull bounding sphere ONLY if this is not an upscaled entity.
	if (!entity->nonNormalizedAxes)
	{
		if (frameIndex == oldFrameIndex)
		{
			switch (cameraFrustum.clipSphere(modelMatrix.transform(frame.position), frame.radius))
			{
			case Frustum::ClipResult::Outside:
				return true;

			case Frustum::ClipResult::Inside:
				return false;
			}
		}
		else
		{
			Frustum::ClipResult cr1 = cameraFrustum.clipSphere(modelMatrix.transform(frame.position), frame.radius);
			Frustum::ClipResult cr2 = cameraFrustum.clipSphere(modelMatrix.transform(oldFrame.position), oldFrame.radius);

			if (cr1 == cr2)
			{
				if (cr1 == Frustum::ClipResult::Outside)
					return true;
				else if (cr1 == Frustum::ClipResult::Inside)
					return false;
			}
		}
	}

	return cameraFrustum.clipBounds(Bounds::merge(frame.bounds, oldFrame.bounds), modelMatrix) == Frustum::ClipResult::Outside;
}

int Model_md3::lerpTag(const char *name, const Entity &entity, int startIndex, Transform *transform) const
{
	assert(transform);
	Transform from, to;

	const int tagIndex = getTag(name, entity.oldFrame, startIndex, &from);

	if (tagIndex < 0 || getTag(name, entity.frame, startIndex, &to) < 0)
		return -1;

	transform->position = vec3::lerp(from.position, to.position, entity.lerp);
	transform->rotation[0] = vec3::lerp(from.rotation[0], to.rotation[0], entity.lerp).normal();
	transform->rotation[1] = vec3::lerp(from.rotation[1], to.rotation[1], entity.lerp).normal();
	transform->rotation[2] = vec3::lerp(from.rotation[2], to.rotation[2], entity.lerp).normal();
	return tagIndex;
}

void Model_md3::render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	assert(entity);

	// Can't render models with no geometry.
	if (!bgfx::isValid(indexBuffer_.handle))
		return;

	// It is possible to have a bad frame while changing models.
	const int frameIndex = Clamped(entity->frame, 0, (int)frames_.size() - 1);
	const int oldFrameIndex = Clamped(entity->oldFrame, 0, (int)frames_.size() - 1);
	const mat4 modelMatrix = mat4::transform(entity->rotation, entity->position);
	const bool isAnimated = frames_.size() > 1;
	bgfx::TransientVertexBuffer tvb;
	Vertex *vertices = nullptr;

	if (isAnimated)
	{
		// Build transient vertex buffer for animated models.
		if (bgfx::getAvailTransientVertexBuffer(nVertices_, Vertex::decl) < nVertices_)
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientVertexBuffer(&tvb, nVertices_, Vertex::decl);
		vertices = (Vertex *)tvb.data;

		// Lerp vertices.
		for (size_t i = 0; i < nVertices_; i++)
		{
			Vertex &fromVertex = frames_[oldFrameIndex].vertices[i];
			Vertex &toVertex = frames_[frameIndex].vertices[i];
			const float fraction = entity->lerp;
			vertices[i].pos = vec3::lerp(fromVertex.pos, toVertex.pos, fraction);
			vertices[i].setNormal(vec3::lerp(fromVertex.getNormal(), toVertex.getNormal(), fraction).normal());
			vertices[i].setTexCoord(toVertex.getTexCoord());
			vertices[i].color = toVertex.color;
		}
	}

	int fogIndex = -1;

	if (world::IsLoaded())
	{
		if (isAnimated)
		{
			const Frame &frame = frames_[oldFrameIndex];
			fogIndex = world::FindFogIndex(vec3(entity->position) + frame.position, frame.radius);
		}
		else
		{
			fogIndex = world::FindFogIndex(entity->position, frames_[0].radius);
		}
	}

	for (Surface &surface : surfaces_)
	{
		Material *mat = surface.materials[0];

		if (entity->customMaterial > 0)
		{
			mat = g_materialCache->getMaterial(entity->customMaterial);
		}
		else if (entity->customSkin > 0)
		{
			Skin *skin = g_materialCache->getSkin(entity->customSkin);
			Material *customMat = skin ? skin->findMaterial(surface.name) : nullptr;

			if (customMat)
				mat = customMat;
		}

		DrawCall dc;
		dc.entity = entity;
		dc.fogIndex = fogIndex;
		dc.material = mat;
		dc.modelMatrix = modelMatrix;

		if ((entity->flags & EntityFlags::DepthHack) != 0)
		{
			if (bgfx::getRendererType() == bgfx::RendererType::Direct3D11)
			{
				dc.zOffset = 0.2f;
			}

			dc.zScale = 0.3f;
		}

		if (isAnimated)
		{
			dc.vb.type = DrawCall::BufferType::Transient;
			dc.vb.transientHandle = tvb;

			// Handle CPU deforms.
			if (isAnimated && mat->hasAutoSpriteDeform())
			{
				bgfx::TransientIndexBuffer tib;

				if (bgfx::getAvailTransientIndexBuffer(surface.nIndices) < surface.nIndices)
				{
					WarnOnce(WarnOnceId::TransientBuffer);
					continue;
				}

				bgfx::allocTransientIndexBuffer(&tib, surface.nIndices);
				memcpy(tib.data, &indices_[surface.startIndex], sizeof(uint16_t) * surface.nIndices);
				mat->doAutoSpriteDeform(sceneRotation, (Vertex *)tvb.data, nVertices_, (uint16_t *)tib.data, surface.nIndices, &dc.softSpriteDepth);
				dc.ib.type = DrawCall::BufferType::Transient;
				dc.ib.transientHandle = tib;
				dc.ib.nIndices = surface.nIndices;
			}
			else
			{
				dc.ib.type = DrawCall::BufferType::Static;
				dc.ib.staticHandle = indexBuffer_.handle;
				dc.ib.firstIndex = surface.startIndex;
				dc.ib.nIndices = surface.nIndices;
			}
		}
		else
		{
			dc.vb.type = DrawCall::BufferType::Static;
			dc.vb.staticHandle = vertexBuffer_.handle;
			dc.ib.type = DrawCall::BufferType::Static;
			dc.ib.staticHandle = indexBuffer_.handle;
			dc.ib.firstIndex = surface.startIndex;
			dc.ib.nIndices = surface.nIndices;
		}

		dc.vb.nVertices = nVertices_;
		drawCallList->push_back(dc);
	}
}

vec3 Model_md3::decodeNormal(short normal) const
{
	// decode X as cos( lat ) * sin( long )
	// decode Y as sin( lat ) * sin( long )
	// decode Z as cos( long )
	unsigned lat = (normal >> 8) & 0xff;
	unsigned lng = (normal & 0xff);
	lat *= (g_funcTableSize / 256);
	lng *= (g_funcTableSize / 256);
	vec3 result;
	result.x = g_sinTable[(lat + (g_funcTableSize / 4)) & g_funcTableMask] * g_sinTable[lng];
	result.y = g_sinTable[lat] * g_sinTable[lng];
	result.z = g_sinTable[(lng + (g_funcTableSize / 4)) & g_funcTableMask];
	return result;
}

int Model_md3::getTag(const char *name, int frame, int startIndex, Transform *transform) const
{
	assert(transform);

	// It is possible to have a bad frame while changing models, so don't error.
	frame = std::min(frame, int(frames_.size() - 1));

	for (int i = 0; i < (int)tagNames_.size(); i++)
	{
		if (i >= startIndex && !strcmp(tagNames_[i].name, name))
		{
			*transform = frames_[frame].tags[i];
			return i;
		}
	}

	return -1;
}

} // namespace renderer
