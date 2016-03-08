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

ModelCache::ModelCache() : hashTable_()
{
}

Model *ModelCache::findModel(const char *name)
{
	if (!name || !name[0])
	{
		ri.Printf(PRINT_ALL, "ModelCache::findModel: NULL name\n");
		return nullptr;
	}

	if (strlen(name) >= MAX_QPATH)
	{
		ri.Printf(PRINT_ALL, "Model name exceeds MAX_QPATH\n");
		return nullptr;
	}

	// Search the currently loaded models
	size_t hash = generateHash(name, hashTableSize_);

	for (Model *m = hashTable_[hash]; m; m = m->next_)
	{
		if (!util::Stricmp(m->name_, name))
		{
			// match found
			return m;
		}
	}

	// Create/load the model
	std::unique_ptr<Model> m = Model::createMD3(name);

	if (!m->load())
	{
		return nullptr;
	}

	return addModel(std::move(m));
}

Model *ModelCache::addModel(std::unique_ptr<Model> model)
{
	size_t hash = generateHash(model->getName(), hashTableSize_);
	model->index_ = models_.size() + 1; // 0 is reserved for missing model/debug axis.
	model->next_ = hashTable_[hash];
	hashTable_[hash] = model.get();
	models_.push_back(std::move(model));
	main::OnModelCreate(hashTable_[hash]);
	return hashTable_[hash];
}

size_t ModelCache::generateHash(const char *fname, size_t size)
{
	size_t hash = 0;
	int i = 0;

	while (fname[i] != '\0')
	{
		char letter = tolower(fname[i]);
		if (letter =='.') break; // don't include extension
		if (letter =='\\') letter = '/'; // damn path names
		if (letter == PATH_SEP) letter = '/'; // damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}

	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}

} // namespace renderer
