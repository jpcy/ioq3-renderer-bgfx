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
/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "Precompiled.h"
#pragma hdrstop

#if defined(ENGINE_IORTCW)

namespace renderer {

/*
==============================================================================

MDS file format (Wolfenstein Skeletal Format)

==============================================================================
*/

#define MDS_IDENT           ( ( 'W' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'M' )
#define MDS_VERSION         4
#define MDS_MAX_VERTS       6000
#define MDS_MAX_TRIANGLES   8192
#define MDS_MAX_BONES       128
#define MDS_MAX_SURFACES    32
#define MDS_MAX_TAGS        128

#define MDS_TRANSLATION_SCALE   ( 1.0 / 64 )

typedef struct {
	int boneIndex;              // these are indexes into the boneReferences,
	float boneWeight;           // not the global per-frame bone list
	vec3 offset;
} mdsWeight_t;

typedef struct {
	vec3 normal;
	vec2 texCoords;
	int numWeights;
	int fixedParent;            // stay equi-distant from this parent
	float fixedDist;
	mdsWeight_t weights[1];     // variable sized
} mdsVertex_t;

typedef struct {
	int indexes[3];
} mdsTriangle_t;

typedef struct {
	int ident;

	char name[MAX_QPATH];           // polyset name
	char shader[MAX_QPATH];
	int shaderIndex;                // for in-game use

	int minLod;

	int ofsHeader;                  // this will be a negative number

	int numVerts;
	int ofsVerts;

	int numTriangles;
	int ofsTriangles;

	int ofsCollapseMap;           // numVerts * int

									// Bone references are a set of ints representing all the bones
									// present in any vertex weights for this surface.  This is
									// needed because a model may have surfaces that need to be
									// drawn at different sort times, and we don't want to have
									// to re-interpolate all the bones for each surface.
	int numBoneReferences;
	int ofsBoneReferences;

	int ofsEnd;                     // next surface follows
} mdsSurface_t;

typedef struct {
	//float		angles[3];
	//float		ofsAngles[2];
	short angles[4];            // to be converted to axis at run-time (this is also better for lerping)
	short ofsAngles[2];         // PITCH/YAW, head in this direction from parent to go to the offset position
} mdsBoneFrameCompressed_t;

typedef struct {
	Bounds bounds;               // bounds of all surfaces of all LOD's for this frame
	vec3 localOrigin;             // midpoint of bounds, used for sphere cull
	float radius;                   // dist from localOrigin to corner
	vec3 parentOffset;            // one bone is an ascendant of all other bones, it starts the hierachy at this position
	mdsBoneFrameCompressed_t bones[1];              // [numBones]
} mdsFrame_t;

typedef struct {
	int numSurfaces;
	int ofsSurfaces;                // first surface, others follow
	int ofsEnd;                     // next lod follows
} mdsLOD_t;

typedef struct {
	char name[MAX_QPATH];           // name of tag
	float torsoWeight;
	int boneIndex;                  // our index in the bones
} mdsTag_t;

#define BONEFLAG_TAG        1       // this bone is actually a tag

typedef struct {
	char name[MAX_QPATH];           // name of bone
	int parent;                     // not sure if this is required, no harm throwing it in
	float torsoWeight;              // scale torso rotation about torsoParent by this
	float parentDist;
	int flags;
} mdsBoneInfo_t;

typedef struct {
	int ident;
	int version;

	char name[MAX_QPATH];           // model name

	float lodScale;
	float lodBias;

	// frames and bones are shared by all levels of detail
	int numFrames;
	int numBones;
	int ofsFrames;                  // mdsFrame_t[numFrames]
	int ofsBones;                   // mdsBoneInfo_t[numBones]
	int torsoParent;                // index of bone that is the parent of the torso

	int numSurfaces;
	int ofsSurfaces;

	// tag data
	int numTags;
	int ofsTags;                    // mdsTag_t[numTags]

	int ofsEnd;                     // end of file
} mdsHeader_t;

class Model_mds : public Model
{
public:
	Model_mds(const char *name);
	bool load(const ReadOnlyFile &file) override;
	Bounds getBounds() const override;
	Material *getMaterial(size_t surfaceNo) const override { return nullptr; }
	bool isCulled(Entity *entity, const Frustum &cameraFrustum) const override;
	int lerpTag(const char *name, const Entity &entity, int startIndex, Transform *transform) const override;
	void render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity) override;

private:
	struct Bone
	{
		mat3 rotation;
		vec3 translation;
	};

	struct Skeleton
	{
		Bone bones[MDS_MAX_BONES];
		bool boneCalculated[MDS_MAX_BONES] = { false };
		const mdsFrame_t *frame, *oldFrame;
		const mdsFrame_t *torsoFrame, *oldTorsoFrame;
		float frontLerp, backLerp;
		float torsoFrontLerp, torsoBackLerp;
	};

	void recursiveBoneListAdd(int boneIndex, int *boneList, int *nBones) const;
	Bone calculateBoneRaw(const Entity &entity, int boneIndex, const Skeleton &skeleton) const;
	Bone calculateBoneLerp(const Entity &entity, int boneIndex, const Skeleton &skeleton) const;
	Bone calculateBone(const Entity &entity, int boneIndex, const Skeleton &skeleton, bool lerp) const;
	Skeleton calculateSkeleton(const Entity &entity, int *boneList, int nBones) const;

	std::vector<uint8_t> data_;
	const mdsHeader_t *header_;
	const mdsBoneInfo_t *boneInfo_;
	std::vector<const mdsFrame_t *> frames_; // Need to access frames by index.
	std::vector<Material *> surfaceMaterials_;
	const mdsTag_t *tags_;
};

std::unique_ptr<Model> Model::createMDS(const char *name)
{
	return std::make_unique<Model_mds>(name);
}

Model_mds::Model_mds(const char *name)
{
	util::Strncpyz(name_, name, sizeof(name_));
}

bool Model_mds::load(const ReadOnlyFile &file)
{
	data_.resize(file.getLength());
	memcpy(data_.data(), file.getData(), file.getLength());

	// Header
	header_ = (mdsHeader_t *)data_.data();

	if (header_->ident != MDS_IDENT)
	{
		interface::PrintWarningf("Model %s: wrong ident (%i should be %i)\n", name_, header_->ident, MDS_IDENT);
		return false;
	}

	if (header_->version != MDS_VERSION)
	{
		interface::PrintWarningf("Model %s: wrong version (%i should be %i)\n", name_, header_->version, MDS_VERSION);
		return false;
	}

	if (header_->numFrames < 1)
	{
		interface::PrintWarningf("Model %s: no frames\n", name_);
		return false;
	}

	boneInfo_ = (mdsBoneInfo_t *)(data_.data() + header_->ofsBones);
	frames_.resize(header_->numFrames);

	for (size_t i = 0; i < frames_.size(); i++)
	{
		const size_t frameSize = sizeof(mdsFrame_t) - sizeof(mdsBoneFrameCompressed_t) + header_->numBones * sizeof(mdsBoneFrameCompressed_t);
		frames_[i] = (mdsFrame_t *)(data_.data() + header_->ofsFrames + i * frameSize);
	}

	surfaceMaterials_.resize(header_->numSurfaces);
	auto surface = (mdsSurface_t *)(data_.data() + header_->ofsSurfaces);

	for (size_t i = 0; i < surfaceMaterials_.size(); i++)
	{
		surfaceMaterials_[i] = surface->shader[0] ? g_materialCache->findMaterial(surface->shader, MaterialLightmapId::None) : nullptr;
		surface = (mdsSurface_t *)((uint8_t *)surface + surface->ofsEnd);
	}

	tags_ = (mdsTag_t *)(data_.data() + header_->ofsTags);
	return true;
}

Bounds Model_mds::getBounds() const
{
	return Bounds();
}

bool Model_mds::isCulled(Entity *entity, const Frustum &cameraFrustum) const
{
	assert(entity);
	return false;
}

int Model_mds::lerpTag(const char *name, const Entity &entity, int startIndex, Transform *transform) const
{
	assert(transform);

	for (int i = 0; i < header_->numTags; i++)
	{
		const mdsTag_t &tag = tags_[i];

		if (i >= startIndex && !strcmp(tags_[i].name, name))
		{
			// Now build the list of bones we need to calc to get this tag's bone information.
			int boneList[MDS_MAX_BONES];
			int nBones = 0;
			recursiveBoneListAdd(tags_[i].boneIndex, boneList, &nBones);

			// Calculate the skeleton.
			Skeleton skeleton = calculateSkeleton(entity, boneList, nBones);

			// Now extract the transform for the bone that represents our tag.
			transform->position = skeleton.bones[tag.boneIndex].translation;
			transform->rotation = skeleton.bones[tag.boneIndex].rotation;
			return i;
		}
	}

	return -1;
}

void Model_mds::render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	assert(entity);

	// It is possible to have a bad frame while changing models.
	const int frameIndex = Clamped(entity->frame, 0, (int)frames_.size() - 1);
	const int oldFrameIndex = Clamped(entity->oldFrame, 0, (int)frames_.size() - 1);
	const mat4 modelMatrix = mat4::transform(entity->rotation, entity->position);

	auto header = (mdsHeader_t *)data_.data();
	auto surface = (mdsSurface_t *)(data_.data() + header->ofsSurfaces);

	for (int i = 0; i < header->numSurfaces; i++)
	{
		Material *mat = surfaceMaterials_[i];

		if (entity->customMaterial > 0)
		{
			mat = g_materialCache->getMaterial(entity->customMaterial);
		}
		else if (entity->customSkin > 0)
		{
			Skin *skin = g_materialCache->getSkin(entity->customSkin);
			Material *customMat = skin ? skin->findMaterial(surface->name) : nullptr;

			if (customMat)
				mat = customMat;
		}

		bgfx::TransientIndexBuffer tib;
		bgfx::TransientVertexBuffer tvb;
		assert(surface->numVerts > 0);
		assert(surface->numTriangles > 0);

		if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, surface->numVerts, &tib, surface->numTriangles * 3))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		auto indices = (uint16_t *)tib.data;
		auto vertices = (Vertex *)tvb.data;
		auto mdsIndices = (const int *)((uint8_t *)surface + surface->ofsTriangles);

		for (int i = 0; i < surface->numTriangles * 3; i++)
		{
			indices[i] = mdsIndices[i];
		}

		Skeleton skeleton = calculateSkeleton(*entity, (int *)((uint8_t *)surface + surface->ofsBoneReferences), surface->numBoneReferences);
		auto mdsVertex = (const mdsVertex_t *)((uint8_t *)surface + surface->ofsVerts);

		for (int i = 0; i < surface->numVerts; i++)
		{
			Vertex &v = vertices[i];
			v.pos = vec3::empty;

			for (int j = 0; j < mdsVertex->numWeights; j++)
			{
				const mdsWeight_t &weight = mdsVertex->weights[j];
				const Bone &bone = skeleton.bones[weight.boneIndex];
				v.pos += (bone.translation + bone.rotation.transform(weight.offset)) * weight.boneWeight;
			}
			
			v.normal = mdsVertex->normal;
			v.texCoord = mdsVertex->texCoords;
			v.color = vec4::white;

			// Move to the next vertex.
			mdsVertex = (mdsVertex_t *)&mdsVertex->weights[mdsVertex->numWeights];
		}

		DrawCall dc;
		dc.entity = entity;
		dc.fogIndex = -1;
		dc.material = mat;
		dc.modelMatrix = modelMatrix;
		dc.vb.type = DrawCall::BufferType::Transient;
		dc.vb.transientHandle = tvb;
		dc.vb.nVertices = surface->numVerts;
		dc.ib.type = DrawCall::BufferType::Transient;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = surface->numTriangles * 3;
		drawCallList->push_back(dc);

		// Move to the next surface.
		surface = (mdsSurface_t *)((uint8_t *)surface + surface->ofsEnd);
	}
}

void Model_mds::recursiveBoneListAdd(int boneIndex, int *boneList, int *nBones) const
{
	assert(boneList);
	assert(nBones);

	if (boneInfo_[boneIndex].parent >= 0)
	{
		recursiveBoneListAdd(boneInfo_[boneIndex].parent, boneList, nBones);
	}

	boneList[(*nBones)++] = boneIndex;
}

static mat4 Matrix4Transform(const mat3 &rotation, vec3 translation)
{
	// mat4::transform translation is 12,13,14
	mat4 m;
	m[0] = rotation[0][0]; m[4] = rotation[1][0]; m[8] = rotation[2][0];  m[12] = 0;
	m[1] = rotation[0][1]; m[5] = rotation[1][1]; m[9] = rotation[2][1];  m[13] = 0;
	m[2] = rotation[0][2]; m[6] = rotation[1][2]; m[10] = rotation[2][2]; m[14] = 0;
	m[3] = translation[0]; m[7] = translation[1]; m[11] = translation[2]; m[15] = 1;
	return m;
}

static void Matrix4Extract(const mat4 &m, mat3 *rotation, vec3 *translation)
{
	m.extract(rotation, nullptr);
	(*translation)[0] = m[3];
	(*translation)[1] = m[7];
	(*translation)[2] = m[11];
}

#define SHORT2ANGLE( x )  ( ( x ) * ( 360.0f / 65536 ) )

/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
static float AngleNormalize360(float angle) {
	return (360.0f / 65536) * ((int)(angle * (65536 / 360.0f)) & 65535);
}

/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
static float AngleNormalize180(float angle) {
	angle = AngleNormalize360(angle);
	if (angle > 180.0) {
		angle -= 360.0;
	}
	return angle;
}

Model_mds::Bone Model_mds::calculateBoneRaw(const Entity &entity, int boneIndex, const Skeleton &skeleton) const
{
	const mdsBoneInfo_t &bi = boneInfo_[boneIndex];
	bool isTorso = false, fullTorso = false;
	const mdsBoneFrameCompressed_t *compressedTorsoBone = nullptr;

	if (bi.torsoWeight)
	{
		compressedTorsoBone = &skeleton.torsoFrame->bones[boneIndex];
		isTorso = true;

		if (bi.torsoWeight == 1.0f)
		{
			fullTorso = true;
		}
	}

	const mdsBoneFrameCompressed_t &compressedBone = skeleton.frame->bones[boneIndex];
	Bone bone;

	// we can assume the parent has already been uncompressed for this frame + lerp
	const Bone *parentBone = nullptr;

	if (bi.parent >= 0)
	{
		parentBone = &skeleton.bones[bi.parent];
	}

	// rotation
	vec3 angles;

	if (fullTorso)
	{
		for (int i = 0; i < 3; i++)
			angles[i] = SHORT2ANGLE(compressedTorsoBone->angles[i]);
	}
	else
	{
		for (int i = 0; i < 3; i++)
			angles[i] = SHORT2ANGLE(compressedBone.angles[i]);

		if (isTorso)
		{
			vec3 torsoAngles;

			for (int i = 0; i < 3; i++)
				torsoAngles[i] = SHORT2ANGLE(compressedTorsoBone->angles[i]);

			// blend the angles together
			for (int i = 0; i < 3; i++)
			{
				float diff = torsoAngles[i] - angles[i];

				if (fabs(diff) > 180)
					diff = AngleNormalize180(diff);

				angles[i] = angles[i] + bi.torsoWeight * diff;
			}
		}
	}

	bone.rotation = mat3(angles);

	// translation
	if (parentBone)
	{
		vec3 vec;

		if (fullTorso)
		{
			angles[0] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[0]);
			angles[1] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[1]);
			angles[2] = 0;
			angles.toAngleVectors(&vec);
		}
		else
		{
			angles[0] = SHORT2ANGLE(compressedBone.ofsAngles[0]);
			angles[1] = SHORT2ANGLE(compressedBone.ofsAngles[1]);
			angles[2] = 0;
			angles.toAngleVectors(&vec);

			if (isTorso)
			{
				vec3 torsoAngles;
				torsoAngles[0] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[0]);
				torsoAngles[1] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[1]);
				torsoAngles[2] = 0;
				vec3 v2;
				torsoAngles.toAngleVectors(&v2);

				// blend the angles together
				vec = vec3::lerp(vec, v2, bi.torsoWeight);
			}
		}

		bone.translation = parentBone->translation + vec * bi.parentDist;
	}
	else // just use the frame position
	{
		bone.translation = frames_[entity.frame]->parentOffset;
	}

	return bone;
}

Model_mds::Bone Model_mds::calculateBoneLerp(const Entity &entity, int boneIndex, const Skeleton &skeleton) const
{
	const mdsBoneInfo_t &bi = boneInfo_[boneIndex];
	const Bone *parentBone = nullptr;

	if (bi.parent >= 0)
	{
		parentBone = &skeleton.bones[bi.parent];
	}

	bool isTorso = false, fullTorso = false;
	const mdsBoneFrameCompressed_t *compressedTorsoBone = nullptr, *oldCompressedTorsoBone = nullptr;

	if (bi.torsoWeight)
	{
		compressedTorsoBone = &skeleton.torsoFrame->bones[boneIndex];
		oldCompressedTorsoBone = &skeleton.oldTorsoFrame->bones[boneIndex];
		isTorso = true;

		if (bi.torsoWeight == 1.0f)
			fullTorso = true;
	}

	const mdsBoneFrameCompressed_t &compressedBone = skeleton.frame->bones[boneIndex];
	const mdsBoneFrameCompressed_t &oldCompressedBone = skeleton.oldFrame->bones[boneIndex];
	Bone bone;

	// rotation (take into account 170 to -170 lerps, which need to take the shortest route)
	vec3 angles;

	if (fullTorso)
	{
		for (int i = 0; i < 3; i++)
		{
			const float a1 = SHORT2ANGLE(compressedTorsoBone->angles[i]);
			const float a2 = SHORT2ANGLE(oldCompressedTorsoBone->angles[i]);
			const float diff = AngleNormalize180(a1 - a2);
			angles[i] = a1 - skeleton.torsoBackLerp * diff;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			const float a1 = SHORT2ANGLE(compressedBone.angles[i]);
			const float a2 = SHORT2ANGLE(oldCompressedBone.angles[i]);
			const float diff = AngleNormalize180(a1 - a2);
			angles[i] = a1 - skeleton.backLerp * diff;
		}

		if (isTorso)
		{
			vec3 torsoAngles;

			for (int i = 0; i < 3; i++)
			{
				const float a1 = SHORT2ANGLE(compressedTorsoBone->angles[i]);
				const float a2 = SHORT2ANGLE(oldCompressedTorsoBone->angles[i]);
				const float diff = AngleNormalize180(a1 - a2);
				torsoAngles[i] = a1 - skeleton.torsoBackLerp * diff;
			}

			// blend the angles together
			for (int j = 0; j < 3; j++)
			{
				float diff = torsoAngles[j] - angles[j];

				if (fabs(diff) > 180)
					diff = AngleNormalize180(diff);

				angles[j] = angles[j] + bi.torsoWeight * diff;
			}
		}
	}

	bone.rotation = mat3(angles);

	if (parentBone)
	{
		const short *sh1, *sh2;

		if (fullTorso)
		{
			sh1 = compressedTorsoBone->ofsAngles;
			sh2 = oldCompressedTorsoBone->ofsAngles;
		}
		else
		{
			sh1 = compressedBone.ofsAngles;
			sh2 = oldCompressedBone.ofsAngles;
		}

		angles[0] = SHORT2ANGLE(sh1[0]);
		angles[1] = SHORT2ANGLE(sh1[1]);
		angles[2] = 0;
		vec3 v2;
		angles.toAngleVectors(&v2); // new

		angles[0] = SHORT2ANGLE(sh2[0]);
		angles[1] = SHORT2ANGLE(sh2[1]);
		angles[2] = 0;
		vec3 vec;
		angles.toAngleVectors(&vec); // old

		// blend the angles together
		vec3 dir;

		if (fullTorso)
		{
			dir = vec3::lerp(vec, v2, skeleton.torsoFrontLerp);
		}
		else
		{
			dir = vec3::lerp(vec, v2, skeleton.frontLerp);
		}

		// translation
		if (!fullTorso && isTorso)
		{
			// partial legs/torso, need to lerp according to torsoWeight
			// calc the torso frame
			angles[0] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[0]);
			angles[1] = SHORT2ANGLE(compressedTorsoBone->ofsAngles[1]);
			angles[2] = 0;
			vec3 v2;
			angles.toAngleVectors(&v2); // new

			angles[0] = SHORT2ANGLE(oldCompressedTorsoBone->ofsAngles[0]);
			angles[1] = SHORT2ANGLE(oldCompressedTorsoBone->ofsAngles[1]);
			angles[2] = 0;
			vec3 vec;
			angles.toAngleVectors(&vec); // old

			// blend the angles together
			v2 = vec3::lerp(vec, v2, skeleton.torsoFrontLerp);

			// blend the torso/legs together
			dir = vec3::lerp(dir, v2, bi.torsoWeight);
		}

		bone.translation = parentBone->translation + dir * bi.parentDist;
	}
	else
	{
		// just interpolate the frame positions
		const mdsFrame_t *frame = frames_[entity.frame], *oldFrame = frames_[entity.oldFrame];
		bone.translation[0] = skeleton.frontLerp * frame->parentOffset[0] + skeleton.backLerp * oldFrame->parentOffset[0];
		bone.translation[1] = skeleton.frontLerp * frame->parentOffset[1] + skeleton.backLerp * oldFrame->parentOffset[1];
		bone.translation[2] = skeleton.frontLerp * frame->parentOffset[2] + skeleton.backLerp * oldFrame->parentOffset[2];
	}

	return bone;
}

Model_mds::Bone Model_mds::calculateBone(const Entity &entity, int boneIndex, const Skeleton &skeleton, bool lerp) const
{
	return lerp ? calculateBoneLerp(entity, boneIndex, skeleton) : calculateBoneRaw(entity, boneIndex, skeleton);
}

Model_mds::Skeleton Model_mds::calculateSkeleton(const Entity &entity, int *boneList, int nBones) const
{
	assert(boneList);
	Skeleton skeleton;

	if (entity.oldFrame == entity.frame)
	{
		skeleton.backLerp = 0;
		skeleton.frontLerp = 1;
	}
	else
	{
		skeleton.backLerp = 1.0f - entity.lerp;
		skeleton.frontLerp = entity.lerp;
	}

	if (entity.oldTorsoFrame == entity.torsoFrame)
	{
		skeleton.torsoBackLerp = 0;
		skeleton.torsoFrontLerp = 1;
	}
	else
	{
		skeleton.torsoBackLerp = 1.0f - entity.torsoLerp;
		skeleton.torsoFrontLerp = entity.torsoLerp;
	}

	
	skeleton.frame = frames_[entity.frame];
	skeleton.oldFrame = frames_[entity.oldFrame];
	skeleton.torsoFrame = entity.torsoFrame >= 0 && entity.torsoFrame < (int)frames_.size() ? frames_[entity.torsoFrame] : nullptr;
	skeleton.oldTorsoFrame = entity.oldTorsoFrame >= 0 && entity.oldTorsoFrame < (int)frames_.size() ? frames_[entity.oldTorsoFrame] : nullptr;

	// Lerp all the needed bones (torsoParent is always the first bone in the list).
	int *boneRefs = boneList;
	mat3 torsoRotation(entity.torsoRotation);
	torsoRotation.transpose();
	const bool lerp = skeleton.backLerp || skeleton.torsoBackLerp;

	for (int i = 0; i < nBones; i++, boneRefs++)
	{
		if (skeleton.boneCalculated[*boneRefs])
			continue;

		// find our parent, and make sure it has been calculated
		const int parentBoneIndex = boneInfo_[*boneRefs].parent;

		if (parentBoneIndex >= 0 && !skeleton.boneCalculated[parentBoneIndex])
		{
			skeleton.bones[parentBoneIndex] = calculateBone(entity, parentBoneIndex, skeleton, lerp);
			skeleton.boneCalculated[parentBoneIndex] = true;
		}

		skeleton.bones[*boneRefs] = calculateBone(entity, *boneRefs, skeleton, lerp);
		skeleton.boneCalculated[*boneRefs] = true;
	}

	// Get the torso parent.
	vec3 torsoParentOffset;
	boneRefs = boneList;

	for (int i = 0; i < nBones; i++, boneRefs++)
	{
		if (*boneRefs == header_->torsoParent)
		{
			torsoParentOffset = skeleton.bones[*boneRefs].translation;
		}
	}

	// Adjust for torso rotations.
	float torsoWeight = 0;
	boneRefs = boneList;
	mat4 m2;

	for (int i = 0; i < nBones; i++, boneRefs++)
	{
		const mdsBoneInfo_t &bi = boneInfo_[*boneRefs];
		Bone *bone = &skeleton.bones[*boneRefs];

		// add torso rotation
		if (bi.torsoWeight > 0)
		{
			if (!(bi.flags & BONEFLAG_TAG))
			{
				// 1st multiply with the bone->matrix
				// 2nd translation for rotation relative to bone around torso parent offset
				const vec3 t = bone->translation - torsoParentOffset;
				mat4 m1 = Matrix4Transform(bone->rotation, t);
				// 3rd scaled rotation
				// 4th translate back to torso parent offset
				// use previously created matrix if available for the same weight
				if (torsoWeight != bi.torsoWeight)
				{
					mat3 scaledRotation;

					for (int j = 0; j < 3; j++)
					{
						for (int k = 0; k < 3; k++)
						{
							scaledRotation[j][k] = torsoRotation[j][k] * bi.torsoWeight;

							if (j == k)
								scaledRotation[j][k] += 1.0f - bi.torsoWeight;
						}
					}

					m2 = Matrix4Transform(scaledRotation, torsoParentOffset);
					torsoWeight = bi.torsoWeight;
				}

				// multiply matrices to create one matrix to do all calculations
				Matrix4Extract(m1 * m2, &bone->rotation, &bone->translation);
			}
			else // tags require special handling
			{
				// rotate each of the axis by the torsoAngles
				for (int j = 0; j < 3; j++)
					bone->rotation[j] = bone->rotation[j] * (1 - bi.torsoWeight) + torsoRotation.transform(bone->rotation[j]) * bi.torsoWeight;

				// rotate the translation around the torsoParent
				const vec3 t = bone->translation - torsoParentOffset;
				bone->translation = t * (1 - bi.torsoWeight) + torsoRotation.transform(t) * bi.torsoWeight + torsoParentOffset;
			}
		}
	}

	return skeleton;
}

} // namespace renderer

#endif // ENGINE_IORTCW
