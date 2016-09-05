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

#ifdef USE_PROFILER

namespace renderer {
namespace profiler {

struct Entry
{
	const char *name = nullptr; // name not copied, can use pointer comparison
	int64_t startTime;
	std::array<int64_t, 125> samples; // 1 seconds at 125fps
	int currentSample;
	int64_t minSample, maxSample;
	float averageSample;
	int indent;
	uint32_t frame;
};

struct Profiler
{
	uint32_t currentFrame;
	std::array<Entry, 64> entries;
	int nEntries = 0;
	std::array<Entry *, 64> entryFrameStack;
	int nEntriesOnFrameStack = 0;
	int indent = 0;
};

static Profiler s_profiler;

void BeginFrame(uint32_t frameNo)
{
	// Discard entries that weren't in the last frame.
	for (int i = 0; i < s_profiler.nEntries; i++)
	{
		Entry &entry = s_profiler.entries[i];

		if (entry.frame != s_profiler.currentFrame)
			entry.name = nullptr;
	}

	s_profiler.currentFrame = frameNo;
	s_profiler.indent = 0;
	s_profiler.nEntriesOnFrameStack = 0;
}

void Print()
{
	const float toMs = 1000.0f / (float)bx::getHPFrequency();

	for (int i = 0; i < s_profiler.nEntries; i++)
	{
		const Entry &entry = s_profiler.entries[i];

		if (entry.frame != s_profiler.currentFrame)
			continue; // Not in this frame.

		int sample = entry.currentSample - 1;
		if (sample < 0)
			sample = (int)entry.samples.size() - 1;
		main::DebugPrint("%*c%s: current:%0.2f min:%0.2f max:%0.2f average:%0.2f", entry.indent + 1, ' ', entry.name, entry.samples[sample] * toMs, entry.minSample * toMs, entry.maxSample * toMs, entry.averageSample * toMs);
	}
}

static Entry *FindOrCreateEntry(const char *name)
{
	for (int i = 0; i < s_profiler.nEntries; i++)
	{
		Entry *entry = &s_profiler.entries[i];

		if (entry->name == name)
			return entry;
	}

	// Not found, find the first empty one.
	Entry *entry = nullptr;

	for (int i = 0; i < s_profiler.nEntries; i++)
	{
		if (s_profiler.entries[i].name == nullptr)
		{
			entry = &s_profiler.entries[i];
			break;
		}
	}

	// No empty entries, append a new one.
	if (!entry)
	{
		if (s_profiler.nEntries == s_profiler.entries.size())
			return nullptr;

		entry = &s_profiler.entries[s_profiler.nEntries++];
	}

	entry->name = name;
	entry->currentSample = 0;
	entry->samples[0] = entry->minSample = entry->maxSample = 0;
	entry->averageSample = 0;
	entry->indent = 0;
	return entry;
}

void BeginEntry(const char *name)
{
	Entry *entry = FindOrCreateEntry(name);

	if (entry)
	{
		entry->startTime = bx::getHPCounter();
		entry->indent = s_profiler.indent;
		entry->frame = s_profiler.currentFrame;
		s_profiler.entryFrameStack[s_profiler.nEntriesOnFrameStack++] = entry;
		s_profiler.indent++;
	}
}

void EndEntry()
{
	Entry *entry = s_profiler.nEntriesOnFrameStack ? s_profiler.entryFrameStack[--s_profiler.nEntriesOnFrameStack] : nullptr;

	if (entry)
	{
		entry->samples[entry->currentSample] = bx::getHPCounter() - entry->startTime;

		if (++entry->currentSample == entry->samples.size())
		{
			entry->currentSample = 0;
			float total = 0;
			entry->minSample = entry->maxSample = entry->samples[0];

			for (int i = 0; i < (int)entry->samples.size(); i++)
			{
				const int64_t sample = entry->samples[i];
				total += sample;
				entry->minSample = std::min(entry->minSample, sample);
				entry->maxSample = std::max(entry->maxSample, sample);
			}

			entry->averageSample = total / (float)entry->samples.size();
		}

		s_profiler.indent--;
	}
}

} // namespace profiler
} // namespace renderer

#endif // USE_PROFILER

