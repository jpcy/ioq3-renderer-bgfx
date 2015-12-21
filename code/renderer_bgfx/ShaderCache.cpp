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

#include "../../build/Shaders.h"

#define BUNDLE(programId, vname, fname, backend) createBundle(programId, bgfx::makeRef(vname##_vertex_##backend, sizeof(vname##_vertex_##backend)), bgfx::makeRef(fname##_fragment_##backend, sizeof(fname##_fragment_##backend)))

#define ALL_BUNDLES(backend)                                                           \
	BUNDLE(ShaderProgramId::Depth, Depth, Depth, backend);                             \
	BUNDLE(ShaderProgramId::Depth_AlphaTest, Depth, Depth_AlphaTest, backend);         \
	BUNDLE(ShaderProgramId::Fog, Fog, Fog, backend);                                   \
	BUNDLE(ShaderProgramId::Generic, Generic, Generic, backend);                       \
	BUNDLE(ShaderProgramId::Generic_AlphaTest, Generic, Generic_AlphaTest, backend);   \
	BUNDLE(ShaderProgramId::Generic_SoftSprite, Generic, Generic_SoftSprite, backend); \
	                                                                                   \
	if (!BUNDLE(ShaderProgramId::TextureColor, TextureColor, TextureColor, backend))   \
	{                                                                                  \
		ri.Error(ERR_FATAL, "A valid TextureColor shader is required");                \
	}                                                                             

namespace renderer {

void ShaderCache::initialize()
{
	const bgfx::RendererType::Enum backend = bgfx::getCaps()->rendererType;

	if (backend == bgfx::RendererType::OpenGL)
	{
		ALL_BUNDLES(gl)
	}
#ifdef WIN32
	else if (backend == bgfx::RendererType::Direct3D9)
	{
		ALL_BUNDLES(d3d9)
	}
	else if (backend == bgfx::RendererType::Direct3D11)
	{
		ALL_BUNDLES(d3d11)
	}
#endif
}

bgfx::ProgramHandle ShaderCache::getHandle(ShaderProgramId programId) const
{
	return bundles_[(int)programId].program.handle;
}

bool ShaderCache::createBundle(ShaderProgramId programId, const bgfx::Memory *vertexMem, const bgfx::Memory *fragmentMem)
{
	assert(vertexMem);
	assert(fragmentMem);
	Bundle *bundle = &bundles_[(int)programId];
	bundle->vertex.handle = bgfx::createShader(vertexMem);

	if (!bgfx::isValid(bundle->vertex.handle))
		return false;

	bundle->fragment.handle = bgfx::createShader(fragmentMem);

	if (!bgfx::isValid(bundle->fragment.handle))
		return false;

	bundle->program.handle = bgfx::createProgram(bundle->vertex.handle, bundle->fragment.handle);
	return bgfx::isValid(bundle->program.handle);
}

} // namespace renderer
