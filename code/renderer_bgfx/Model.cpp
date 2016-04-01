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

#define LittleFloat(x) (x)
#define LittleLong(x) (x)
#define LittleShort(x) (x)
#define	LL(x) x = LittleLong(x)

namespace renderer {

struct ModelSurface
{
	char name[MAX_QPATH]; // polyset name
	std::vector<Material *> materials;
	uint32_t startIndex;
	uint32_t nIndices;
	uint32_t startVertex;
	uint32_t nVertices;
};

struct ModelTagName
{
	char name[MAX_QPATH];
};

struct ModelFrame
{
	Bounds bounds;
	vec3 position;
	float radius;
	std::vector<Transform> tags;

	/// Vertex data in system memory. Used by animated models.
	std::vector<Vertex> vertices;
};

class Model_md3 : public Model
{
public:
	Model_md3(const char *name);
	bool load() override;
	Bounds getBounds() const override;
	Transform getTag(const char *name, int frame) const override;
	bool isCulled(Entity *entity, const Frustum &cameraFrustum) const override;
	void render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity) override;

private:
	Vertex loadVertex(size_t index, md3St_t *fileTexCoords, md3XyzNormal_t *fileXyzNormals);

	IndexBuffer indexBuffer_;

	/// Need to keep a copy of the model indices in system memory for CPU deforms.
	std::vector<uint16_t> indices_;

	/// Static model vertex buffer.
	VertexBuffer vertexBuffer_;

	/// The number of vertices in all the surfaces of a single frame.
	uint32_t nVertices_;

	std::vector<ModelFrame> frames_;
	std::vector<ModelTagName> tagNames_;
	std::vector<ModelSurface> surfaces_;
};

std::unique_ptr<Model> Model::createMD3(const char *name)
{
	return std::make_unique<Model_md3>(name);
}

Model_md3::Model_md3(const char *name)
{
	util::Strncpyz(name_, name, sizeof(name_));
}

bool Model_md3::load()
{
	ReadOnlyFile file(name_);

	if (!file.isValid())
		return false;

	uint8_t *data = file.getData();

	// Header
	auto fileHeader = (md3Header_t *)data;
	LL(fileHeader->version);

	if (fileHeader->version != MD3_VERSION)
	{
		ri.Printf(PRINT_WARNING, "Model %s has wrong version (%i should be %i)\n", name_, fileHeader->version, MD3_VERSION);
		return false;
	}

	LL(fileHeader->ident);
	LL(fileHeader->version);
	LL(fileHeader->numFrames);
	LL(fileHeader->numTags);
	LL(fileHeader->numSurfaces);
	LL(fileHeader->ofsFrames);
	LL(fileHeader->ofsTags);
	LL(fileHeader->ofsSurfaces);
	LL(fileHeader->ofsEnd);

	if (fileHeader->numFrames < 1)
	{
		ri.Printf(PRINT_WARNING, "Model %s has no frames\n", name_);
		return false;
	}

	// Frames
	auto fileFrames = (const md3Frame_t *)&data[fileHeader->ofsFrames];
	frames_.resize(fileHeader->numFrames);

	for (int i = 0; i < fileHeader->numFrames; i++)
	{
		ModelFrame &frame = frames_[i];
		const md3Frame_t &fileFrame = fileFrames[i];
		frame.radius = LittleFloat(fileFrame.radius);

		for (int j = 0; j < 3; j++)
		{
			frame.bounds[0][j] = LittleFloat(fileFrame.bounds[0][j]);
			frame.bounds[1][j] = LittleFloat(fileFrame.bounds[1][j]);
			frame.position[j] = LittleFloat(fileFrame.localOrigin[j]);
		}

		// Tags
		auto fileTags = (const md3Tag_t *)&data[fileHeader->ofsTags];
		frame.tags.resize(fileHeader->numTags);

		for (int j = 0; j < fileHeader->numTags; j++)
		{
			Transform &tag = frame.tags[j];
			const md3Tag_t &fileTag = fileTags[j + i * fileHeader->numTags];
			
			for (int k = 0; k < 3; k++)
			{
				tag.position[k] = LittleFloat(fileTag.origin[k]);
				tag.rotation[0][k] = LittleFloat(fileTag.axis[0][k]);
				tag.rotation[1][k] = LittleFloat(fileTag.axis[1][k]);
				tag.rotation[2][k] = LittleFloat(fileTag.axis[2][k]);
			}
		}
	}

	// Tag names
	auto fileTags = (const md3Tag_t *)&data[fileHeader->ofsTags];
	tagNames_.resize(fileHeader->numTags);

	for (int i = 0; i < fileHeader->numTags; i++)
	{
		util::Strncpyz(tagNames_[i].name, fileTags[i].name, sizeof(tagNames_[i].name));
	}

	// Surfaces
	auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];
	surfaces_.resize(fileHeader->numSurfaces);

	for (int i = 0; i < fileHeader->numSurfaces; i++)
	{
		ModelSurface &s = surfaces_[i];
		md3Surface_t &fs = *fileSurface; // Just an alias.
		LL(fs.ident);
		LL(fs.flags);
		LL(fs.numFrames);
		LL(fs.numShaders);
		LL(fs.numTriangles);
		LL(fs.ofsTriangles);
		LL(fs.numVerts);
		LL(fs.ofsShaders);
		LL(fs.ofsSt);
		LL(fs.ofsXyzNormals);
		LL(fs.ofsEnd);

		util::Strncpyz(s.name, fs.name, sizeof(s.name));
		util::ToLowerCase(s.name); // Lowercase the surface name so skin compares are faster.

		// Strip off a trailing _1 or _2. This is a crutch for q3data being a mess.
		size_t n = strlen(s.name);

		if (n > 2 && s.name[n - 2] == '_')
		{
			s.name[n - 2] = 0;
		}

		// Surface materials
		auto fileShaders = (md3Shader_t *)((uint8_t *)&fs + fs.ofsShaders);
		s.materials.resize(fs.numShaders);

		for (int j = 0; j < fs.numShaders; j++)
		{
			s.materials[j] = g_materialCache->findMaterial(fileShaders[j].name, MaterialLightmapId::None);
		}

		// Move to the next surface.
		fileSurface = (md3Surface_t *)((uint8_t *)&fs + fs.ofsEnd);
	}

	// Total the number of indices and vertices in each surface.
	size_t nIndices = 0;
	nVertices_ = 0;
	fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

	for (int i = 0; i < fileHeader->numSurfaces; i++)
	{
		nIndices += fileSurface->numTriangles * 3;
		nVertices_ += fileSurface->numVerts;
		fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
	}

	// Stop here if the model doesn't have any geometry (e.g. weapon hand models).
	if (nIndices == 0)
		return true;

	const bool isAnimated = frames_.size() > 1;

	// Merge all surface indices into one index buffer. For each surface, store the start index and number of indices.
	const bgfx::Memory *indicesMem = bgfx::alloc(uint32_t(sizeof(uint16_t) * nIndices));
	auto indices = (uint16_t *)indicesMem->data;
	uint32_t startIndex = 0, startVertex = 0;
	fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

	for (int i = 0; i < fileHeader->numSurfaces; i++)
	{
		ModelSurface &surface = surfaces_[i];
		surface.startIndex = startIndex;
		surface.nIndices = fileSurface->numTriangles * 3;
		auto fileIndices = (int *)((uint8_t *)fileSurface + fileSurface->ofsTriangles);

		for (uint32_t j = 0; j < surface.nIndices; j++)
		{
			indices[startIndex + j] = LittleLong(startVertex + fileIndices[j]);
		}

		startIndex += surface.nIndices;
		startVertex += fileSurface->numVerts;
		fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
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
		auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

		for (int i = 0; i < fileHeader->numSurfaces; i++)
		{
			ModelSurface &surface = surfaces_[i];
			auto fileTexCoords = (md3St_t *)((uint8_t *)fileSurface + fileSurface->ofsSt);
			auto fileXyzNormals = (md3XyzNormal_t *)((uint8_t *)fileSurface + fileSurface->ofsXyzNormals);

			for (int j = 0; j < fileSurface->numVerts; j++)
			{
				vertices[startVertex + j] = loadVertex(j, fileTexCoords, fileXyzNormals);
			}

			startVertex += fileSurface->numVerts;
			fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
		}

		vertexBuffer_.handle = bgfx::createVertexBuffer(verticesMem, Vertex::decl);
	}
	else
	{
		for (int i = 0; i < fileHeader->numFrames; i++)
		{
			frames_[i].vertices.resize(nVertices_);
		}

		uint32_t startVertex = 0;
		auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

		for (int i = 0; i < fileHeader->numSurfaces; i++)
		{
			ModelSurface &surface = surfaces_[i];

			// Texture coords are the same for each frame, positions and normals aren't.
			auto fileTexCoords = (md3St_t *)((uint8_t *)fileSurface + fileSurface->ofsSt);

			for (int j = 0; j < fileHeader->numFrames; j++)
			{
				auto fileXyzNormals = (md3XyzNormal_t *)((uint8_t *)fileSurface + fileSurface->ofsXyzNormals + j * sizeof(md3XyzNormal_t) * fileSurface->numVerts);

				for (int k = 0; k < fileSurface->numVerts; k++)
				{
					frames_[j].vertices[startVertex + k] = loadVertex(k, fileTexCoords, fileXyzNormals);
				}
			}

			surface.startVertex = startVertex;
			surface.nVertices = fileSurface->numVerts;
			startVertex += fileSurface->numVerts;
			fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
		}
	}

	return true;
}

Bounds Model_md3::getBounds() const
{
	return frames_[0].bounds;
}

Transform Model_md3::getTag(const char *name, int frame) const
{
	// It is possible to have a bad frame while changing models, so don't error.
	frame = std::min(frame, int(frames_.size() - 1));

	for (size_t i = 0; i < tagNames_.size(); i++)
	{
		if (!strcmp(tagNames_[i].name, name))
			return frames_[frame].tags[i];
	}

	return Transform();
}

bool Model_md3::isCulled(Entity *entity, const Frustum &cameraFrustum) const
{
	assert(entity);

	// It is possible to have a bad frame while changing models.
	const int frameIndex = Clamped(entity->e.frame, 0, (int)frames_.size() - 1);
	const int oldFrameIndex = Clamped(entity->e.oldframe, 0, (int)frames_.size() - 1);
	const ModelFrame &frame = frames_[frameIndex];
	const ModelFrame &oldFrame = frames_[oldFrameIndex];
	const mat4 modelMatrix = mat4::transform(entity->e.axis, entity->e.origin);

	// Cull bounding sphere ONLY if this is not an upscaled entity.
	if (!entity->e.nonNormalizedAxes)
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

void Model_md3::render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	assert(entity);

	// Can't render models with no geometry.
	if (!bgfx::isValid(indexBuffer_.handle))
		return;

	// It is possible to have a bad frame while changing models.
	const int frameIndex = Clamped(entity->e.frame, 0, (int)frames_.size() - 1);
	const int oldFrameIndex = Clamped(entity->e.oldframe, 0, (int)frames_.size() - 1);
	const mat4 modelMatrix = mat4::transform(entity->e.axis, entity->e.origin);
	const bool isAnimated = frames_.size() > 1;
	bgfx::TransientVertexBuffer tvb;
	Vertex *vertices = nullptr;

	if (isAnimated)
	{
		// Build transient vertex buffer for animated models.
		if (!bgfx::checkAvailTransientVertexBuffer(nVertices_, Vertex::decl))
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
			const float fraction = 1.0f - entity->e.backlerp;
			vertices[i].pos = vec3::lerp(fromVertex.pos, toVertex.pos, fraction);
			vertices[i].normal = vec3::lerp(fromVertex.normal, toVertex.normal, fraction).normal();
			vertices[i].texCoord = toVertex.texCoord;
			vertices[i].color = toVertex.color;
		}
	}

	int fogIndex = -1;

	if (world::IsLoaded())
	{
		if (isAnimated)
		{
			const ModelFrame &frame = frames_[oldFrameIndex];
			fogIndex = world::FindFogIndex(vec3(entity->e.origin) + frame.position, frame.radius);
		}
		else
		{
			fogIndex = world::FindFogIndex(entity->e.origin, frames_[0].radius);
		}
	}

	for (ModelSurface &surface : surfaces_)
	{
		Material *mat = surface.materials[0];

		if (entity->e.customShader > 0)
		{
			mat = g_materialCache->getMaterial(entity->e.customShader);
		}
		else if (entity->e.customSkin > 0)
		{
			Material *customMat = g_materialCache->getSkin(entity->e.customSkin)->findMaterial(surface.name);

			if (customMat)
				mat = customMat;
		}

		DrawCall dc;
		dc.entity = entity;
		dc.fogIndex = fogIndex;
		dc.material = mat;
		dc.modelMatrix = modelMatrix;

		if ((entity->e.renderfx & RF_DEPTHHACK) != 0)
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
				
				if (!bgfx::checkAvailTransientIndexBuffer(surface.nIndices))
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

Vertex Model_md3::loadVertex(size_t index, md3St_t *fileTexCoords, md3XyzNormal_t *fileXyzNormals)
{
	Vertex v;

	// Position
	v.pos.x = LittleShort(fileXyzNormals[index].xyz[0]) * MD3_XYZ_SCALE;
	v.pos.y = LittleShort(fileXyzNormals[index].xyz[1]) * MD3_XYZ_SCALE;
	v.pos.z = LittleShort(fileXyzNormals[index].xyz[2]) * MD3_XYZ_SCALE;

	// Normal
	// decode X as cos( lat ) * sin( long )
	// decode Y as sin( lat ) * sin( long )
	// decode Z as cos( long )
	short normal = LittleShort(fileXyzNormals[index].normal);
	unsigned lat = (normal >> 8) & 0xff;
	unsigned lng = (normal & 0xff);
	lat *= (g_funcTableSize / 256);
	lng *= (g_funcTableSize / 256);
	v.normal.x = g_sinTable[(lat + (g_funcTableSize / 4)) & g_funcTableMask] * g_sinTable[lng];
	v.normal.y = g_sinTable[lat] * g_sinTable[lng];
	v.normal.z = g_sinTable[(lng + (g_funcTableSize / 4)) & g_funcTableMask];

	// UV
	v.texCoord.u = LittleFloat(fileTexCoords[index].st[0]);
	v.texCoord.v = LittleFloat(fileTexCoords[index].st[1]);

	v.color = { 1, 1, 1, 1 };
	return v;
}

} // namespace renderer
