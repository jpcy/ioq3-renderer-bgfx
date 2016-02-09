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

#define	LL(x) x = LittleLong(x)

namespace renderer {

struct ModelSurface
{
	char name[MAX_QPATH]; // polyset name
	std::vector<Material *> materials;
	uint32_t startIndex;
	uint32_t nIndices;
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

	/// Vertex data in system memory. Used by animated models and static models with CPU deforms.
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
	void render(DrawCallList *drawCallList, Entity *entity) override;

private:
	Vertex loadVertex(size_t index, md3St_t *fileTexCoords, md3XyzNormal_t *fileXyzNormals);

	IndexBuffer indexBuffer_;

	/// Static model vertex buffer.
	VertexBuffer vertexBuffer_;

	/// The number of vertices in all the surfaces of a single frame.
	uint32_t nVertices_;

	/// Index data in system memory. Used by static models with CPU deforms.
	std::vector<uint16_t> indices_;

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
	Q_strncpyz(name_, name, sizeof(name_));
}

bool Model_md3::load()
{
	ReadOnlyFile file(name_);

	if (!file.isValid())
		return false;

	auto data = file.getData();

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

	for (size_t i = 0; i < fileHeader->numFrames; i++)
	{
		auto &frame = frames_[i];
		auto &fileFrame = fileFrames[i];
		frame.radius = LittleFloat(fileFrame.radius);

		for (size_t j = 0; j < 3; j++)
		{
			frame.bounds[0][j] = LittleFloat(fileFrame.bounds[0][j]);
			frame.bounds[1][j] = LittleFloat(fileFrame.bounds[1][j]);
			frame.position[j] = LittleFloat(fileFrame.localOrigin[j]);
		}

		// Tags
		auto fileTags = (const md3Tag_t *)&data[fileHeader->ofsTags];
		frame.tags.resize(fileHeader->numTags);

		for (size_t j = 0; j < fileHeader->numTags; j++)
		{
			auto &tag = frame.tags[j];
			auto &fileTag = fileTags[j + i * fileHeader->numTags];
			
			for (size_t k = 0; k < 3; k++)
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

	for (size_t i = 0; i < fileHeader->numTags; i++)
	{
		Q_strncpyz(tagNames_[i].name, fileTags[i].name, sizeof(tagNames_[i].name));
	}

	// Surfaces
	auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];
	surfaces_.resize(fileHeader->numSurfaces);

	for (size_t i = 0; i < fileHeader->numSurfaces; i++)
	{
		auto &s = surfaces_[i];
		auto &fs = *fileSurface; // Just an alias.
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

		Q_strncpyz(s.name, fs.name, sizeof(s.name));
		Q_strlwr(s.name); // Lowercase the surface name so skin compares are faster.

		// Strip off a trailing _1 or _2. This is a crutch for q3data being a mess.
		auto n = strlen(s.name);

		if (n > 2 && s.name[n - 2] == '_')
		{
			s.name[n - 2] = 0;
		}

		// Surface materials
		auto fileShaders = (md3Shader_t *)((uint8_t *)&fs + fs.ofsShaders);
		s.materials.resize(fs.numShaders);

		for (size_t j = 0; j < fs.numShaders; j++)
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

	for (size_t i = 0; i < fileHeader->numSurfaces; i++)
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
	auto indicesMem = bgfx::alloc(uint32_t(sizeof(uint16_t) * nIndices));
	auto indices = (uint16_t *)indicesMem->data;
	uint32_t startIndex = 0, startVertex = 0;
	fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

	if (!isAnimated)
	{
		// Store indices in system memory too, so they can be used by CPU deforms.
		indices_.resize(nIndices);
	}

	for (size_t i = 0; i < fileHeader->numSurfaces; i++)
	{
		auto &surface = surfaces_[i];
		surface.startIndex = startIndex;
		surface.nIndices = fileSurface->numTriangles * 3;
		auto fileIndices = (int *)((uint8_t *)fileSurface + fileSurface->ofsTriangles);

		for (size_t j = 0; j < surface.nIndices; j++)
		{
			indices[startIndex + j] = LittleLong(startVertex + fileIndices[j]);

			// Store indices in system memory too, so they can be used by CPU deforms.
			if (!isAnimated)
			{
				indices_[startIndex + j] = indices[startIndex + j];
			}
		}

		startIndex += surface.nIndices;
		startVertex += fileSurface->numVerts;
		fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
	}

	indexBuffer_.handle = bgfx::createIndexBuffer(indicesMem);

	// Vertices
	// Texture coords are the same for each frame, positions and normals aren't.
	// Static models (models with 1 frame) have their surface vertices merged into a single vertex buffer.
	// Animated models (models with more than 1 frame) have their surface vertices merged into a single system memory vertex array for each frame.
	if (!isAnimated)
	{
		auto verticesMem = bgfx::alloc(sizeof(Vertex) * nVertices_);
		auto vertices = (Vertex *)verticesMem->data;
		frames_[0].vertices.resize(nVertices_);
		size_t startVertex = 0;
		auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

		for (size_t i = 0; i < fileHeader->numSurfaces; i++)
		{
			auto fileTexCoords = (md3St_t *)((uint8_t *)fileSurface + fileSurface->ofsSt);
			auto fileXyzNormals = (md3XyzNormal_t *)((uint8_t *)fileSurface + fileSurface->ofsXyzNormals);

			for (size_t j = 0; j < fileSurface->numVerts; j++)
			{
				vertices[startVertex + j] = loadVertex(j, fileTexCoords, fileXyzNormals);

				// Store vertices in system memory too, so they can be used by CPU deforms.
				frames_[0].vertices[startVertex + j] = vertices[startVertex + j];
			}

			startVertex += fileSurface->numVerts;
			fileSurface = (md3Surface_t *)((uint8_t *)fileSurface + fileSurface->ofsEnd);
		}

		vertexBuffer_.handle = bgfx::createVertexBuffer(verticesMem, Vertex::decl);
	}
	else
	{
		for (size_t i = 0; i < fileHeader->numFrames; i++)
		{
			frames_[i].vertices.resize(nVertices_);
		}

		size_t startVertex = 0;
		auto fileSurface = (md3Surface_t *)&data[fileHeader->ofsSurfaces];

		for (size_t i = 0; i < fileHeader->numSurfaces; i++)
		{
			// Texture coords are the same for each frame, positions and normals aren't.
			auto fileTexCoords = (md3St_t *)((uint8_t *)fileSurface + fileSurface->ofsSt);

			for (size_t j = 0; j < fileHeader->numFrames; j++)
			{
				auto fileXyzNormals = (md3XyzNormal_t *)((uint8_t *)fileSurface + fileSurface->ofsXyzNormals + j * sizeof(md3XyzNormal_t) * fileSurface->numVerts);

				for (size_t k = 0; k < fileSurface->numVerts; k++)
				{
					frames_[j].vertices[startVertex + k] = loadVertex(k, fileTexCoords, fileXyzNormals);
				}
			}

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
	if (entity->e.frame >= frames_.size() || entity->e.oldframe >= frames_.size())
		return true;

	const auto &frame = frames_[entity->e.frame];
	const auto &oldFrame = frames_[entity->e.oldframe];
	const auto modelMatrix = mat4::transform(entity->e.axis, entity->e.origin);

	// Cull bounding sphere ONLY if this is not an upscaled entity.
	if (!entity->e.nonNormalizedAxes)
	{
		if (entity->e.frame == entity->e.oldframe)
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

void Model_md3::render(DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	assert(entity);

	// Can't render models with no geometry.
	if (!bgfx::isValid(indexBuffer_.handle))
		return;

	// It is possible to have a bad frame while changing models.
	if (entity->e.frame >= frames_.size() || entity->e.oldframe >= frames_.size())
		return;

	const auto modelMatrix = mat4::transform(entity->e.axis, entity->e.origin);
	const bool isAnimated = frames_.size() > 1;
	bgfx::TransientVertexBuffer tvb;

	// Build transient vertex buffer for animated models.
	if (isAnimated)
	{
		if (!bgfx::checkAvailTransientVertexBuffer(nVertices_, Vertex::decl))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientVertexBuffer(&tvb, nVertices_, Vertex::decl);
		auto vertices = (Vertex *)tvb.data;

		// Lerp vertices.
		for (size_t i = 0; i < nVertices_; i++)
		{
			auto &fromVertex = frames_[entity->e.oldframe].vertices[i];
			auto &toVertex = frames_[entity->e.frame].vertices[i];

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
			const auto &frame = frames_[entity->e.oldframe];
			fogIndex = world::FindFogIndex(vec3(entity->e.origin) + frame.position, frame.radius);
		}
		else
		{
			fogIndex = world::FindFogIndex(entity->e.origin, frames_[0].radius);
		}
	}

	for (auto &surface : surfaces_)
	{
		Material *mat = surface.materials[0];

		if (entity->e.customShader > 0)
		{
			mat = g_materialCache->getMaterial(entity->e.customShader);
		}
		else if (entity->e.customSkin > 0)
		{
			auto customMat = g_materialCache->getSkin(entity->e.customSkin)->findMaterial(surface.name);

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
			dc.zScale = 0.3f;
		}

		if (!isAnimated && mat->hasCpuDeforms())
		{
			bgfx::TransientVertexBuffer stvb;
			bgfx::TransientIndexBuffer stib;

			if (!bgfx::allocTransientBuffers(&stvb, Vertex::decl, nVertices_, &stib, surface.nIndices))
			{
				WarnOnce(WarnOnceId::TransientBuffer);
				continue;
			}

			memcpy(stvb.data, frames_[0].vertices.data(), stvb.size);
			memcpy(stib.data, &indices_[surface.startIndex], stib.size);
			dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
			dc.vb.transientHandle = stvb;
			dc.vb.nVertices = nVertices_;
			dc.ib.transientHandle = stib;
			dc.ib.nIndices = surface.nIndices;
		}
		else
		{
			if (isAnimated)
			{
				dc.vb.type = DrawCall::BufferType::Transient;
				dc.vb.transientHandle = tvb;
			}
			else
			{
				dc.vb.type = DrawCall::BufferType::Static;
				dc.vb.staticHandle = vertexBuffer_.handle;
			}
		
			dc.vb.nVertices = nVertices_;
			dc.ib.type = DrawCall::BufferType::Static;
			dc.ib.staticHandle = indexBuffer_.handle;
			dc.ib.firstIndex = surface.startIndex;
			dc.ib.nIndices = surface.nIndices;
		}

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
	auto normal = LittleShort(fileXyzNormals[index].normal);
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
