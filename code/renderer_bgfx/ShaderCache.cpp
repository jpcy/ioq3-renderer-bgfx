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

#ifdef WIN32
#include "../../build/shaders/Fog_fragment_d3d9.h"
#include "../../build/shaders/Fog_vertex_d3d9.h"
#include "../../build/shaders/Generic_AlphaTest_fragment_d3d9.h"
#include "../../build/shaders/Generic_fragment_d3d9.h"
#include "../../build/shaders/Generic_vertex_d3d9.h"
#include "../../build/shaders/TextureColor_fragment_d3d9.h"
#include "../../build/shaders/TextureColor_vertex_d3d9.h"

#include "../../build/shaders/Fog_fragment_d3d11.h"
#include "../../build/shaders/Fog_vertex_d3d11.h"
#include "../../build/shaders/Generic_AlphaTest_fragment_d3d11.h"
#include "../../build/shaders/Generic_fragment_d3d11.h"
#include "../../build/shaders/Generic_vertex_d3d11.h"
#include "../../build/shaders/TextureColor_fragment_d3d11.h"
#include "../../build/shaders/TextureColor_vertex_d3d11.h"
#endif

#include "../../build/shaders/Fog_fragment_gl.h"
#include "../../build/shaders/Fog_vertex_gl.h"
#include "../../build/shaders/Generic_AlphaTest_fragment_gl.h"
#include "../../build/shaders/Generic_fragment_gl.h"
#include "../../build/shaders/Generic_vertex_gl.h"
#include "../../build/shaders/TextureColor_fragment_gl.h"
#include "../../build/shaders/TextureColor_vertex_gl.h"

#define BUNDLE(programId, vname, fname, backend) createBundle(programId, bgfx::makeRef(vname##_vertex_##backend, sizeof(vname##_vertex_##backend)), bgfx::makeRef(fname##_fragment_##backend, sizeof(fname##_fragment_##backend)))

namespace renderer {

void ShaderCache::initialize()
{
	const bgfx::RendererType::Enum backend = bgfx::getCaps()->rendererType;

	if (backend == bgfx::RendererType::OpenGL)
	{
		BUNDLE(ShaderProgramId::Fog, Fog, Fog, gl);
		BUNDLE(ShaderProgramId::Generic, Generic, Generic, gl);
		BUNDLE(ShaderProgramId::Generic_AlphaTest, Generic, Generic_AlphaTest, gl);

		if (!BUNDLE(ShaderProgramId::TextureColor, TextureColor, TextureColor, gl))
		{
			ri.Error(ERR_FATAL, "A valid TextureColor shader is required");
		}
	}
#ifdef WIN32
	else if (backend == bgfx::RendererType::Direct3D9)
	{
		BUNDLE(ShaderProgramId::Fog, Fog, Fog, d3d9);
		BUNDLE(ShaderProgramId::Generic, Generic, Generic, d3d9);
		BUNDLE(ShaderProgramId::Generic_AlphaTest, Generic, Generic_AlphaTest, d3d9);

		if (!BUNDLE(ShaderProgramId::TextureColor, TextureColor, TextureColor, d3d9))
		{
			ri.Error(ERR_FATAL, "A valid TextureColor shader is required");
		}
	}
	else if (backend == bgfx::RendererType::Direct3D11)
	{
		BUNDLE(ShaderProgramId::Fog, Fog, Fog, d3d11);
		BUNDLE(ShaderProgramId::Generic, Generic, Generic, d3d11);
		BUNDLE(ShaderProgramId::Generic_AlphaTest, Generic, Generic_AlphaTest, d3d11);

		if (!BUNDLE(ShaderProgramId::TextureColor, TextureColor, TextureColor, d3d11))
		{
			ri.Error(ERR_FATAL, "A valid TextureColor shader is required");
		}
	}
#endif
}

bgfx::ProgramHandle ShaderCache::getHandle(ShaderProgramId programId, int flags) const
{
	bgfx::ProgramHandle handle = bundles_[(int)programId].program.handle;

	if (bgfx::isValid(handle) || (flags & GetHandleFlags::ReturnInvalid))
		return handle;
	
	// Fallback to TextureColor shader program.
	return bundles_[(int)ShaderProgramId::TextureColor].program.handle;
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
