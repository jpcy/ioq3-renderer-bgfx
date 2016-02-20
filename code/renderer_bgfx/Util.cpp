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
namespace util {

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;
static	int		com_tokenline;

static char *SkipWhitespace(char *data, bool *hasNewLines)
{
	int c;

	while ((c = *data) <= ' ') {
		if (!c) {
			return NULL;
		}
		if (c == '\n') {
			com_lines++;
			*hasNewLines = true;
		}
		data++;
	}

	return data;
}

void BeginParseSession(const char *name)
{
	com_lines = 1;
	com_tokenline = 0;
	Sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}

int GetCurrentParseLine()
{
	if (com_tokenline)
	{
		return com_tokenline;
	}

	return com_lines;
}

char *Parse(char **data_p, bool allowLineBreaks)
{
	int c = 0, len;
	bool hasNewLines = false;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	com_tokenline = 0;

	// make sure incoming data is valid
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}

	while (1)
	{
		// skip whitespace
		data = SkipWhitespace(data, &hasNewLines);
		if (!data)
		{
			*data_p = NULL;
			return com_token;
		}
		if (hasNewLines && !allowLineBreaks)
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if (c == '/' && data[1] == '/')
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if (c == '/' && data[1] == '*')
		{
			data += 2;
			while (*data && (*data != '*' || data[1] != '/'))
			{
				if (*data == '\n')
				{
					com_lines++;
				}
				data++;
			}
			if (*data)
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// token starts on this line
	com_tokenline = com_lines;

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = (char *)data;
				return com_token;
			}
			if (c == '\n')
			{
				com_lines++;
			}
			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	com_token[len] = 0;

	*data_p = (char *)data;
	return com_token;
}

bool SkipBracedSection(char **program, int depth)
{
	do {
		char *token = Parse(program, true);
		if (token[1] == 0) {
			if (token[0] == '{') {
				depth++;
			}
			else if (token[0] == '}') {
				depth--;
			}
		}
	} while (depth && *program);

	return (depth == 0);
}

void SkipRestOfLine(char **data)
{
	char	*p;
	int		c;

	p = *data;

	if (!*p)
		return;

	while ((c = *p++) != 0) {
		if (c == '\n') {
			com_lines++;
			break;
		}
	}

	*data = p;
}

int Compress(char *data_p)
{
	char *in, *out;
	int c;
	bool newline = false, whitespace = false;

	in = out = data_p;
	if (in) {
		while ((c = *in) != 0) {
			// skip double slash comments
			if (c == '/' && in[1] == '/') {
				while (*in && *in != '\n') {
					in++;
				}
				// skip /* */ comments
			}
			else if (c == '/' && in[1] == '*') {
				while (*in && (*in != '*' || in[1] != '/'))
					in++;
				if (*in)
					in += 2;
				// record when we hit a newline
			}
			else if (c == '\n' || c == '\r') {
				newline = qtrue;
				in++;
				// record when we hit whitespace
			}
			else if (c == ' ' || c == '\t') {
				whitespace = qtrue;
				in++;
				// an actual token
			}
			else {
				// if we have a pending newline, emit it (and it counts as whitespace)
				if (newline) {
					*out++ = '\n';
					newline = qfalse;
					whitespace = qfalse;
				} if (whitespace) {
					*out++ = ' ';
					whitespace = qfalse;
				}

				// copy quoted strings unmolested
				if (c == '"') {
					*out++ = c;
					in++;
					while (1) {
						c = *in;
						if (c && c != '"') {
							*out++ = c;
							in++;
						}
						else {
							break;
						}
					}
					if (c == '"') {
						*out++ = c;
						in++;
					}
				}
				else {
					*out = c;
					out++;
					in++;
				}
			}
		}

		*out = 0;
	}
	return out - data_p;
}

uint16_t CalculateSmallestPowerOfTwoTextureSize(int nPixels)
{
	const int sr = (int)ceil(sqrtf((float)nPixels));
	uint16_t textureSize = 1;

	while (textureSize < sr)
		textureSize *= 2;

	return textureSize;
}

char *SkipPath(char *pathname)
{
	char *last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

const char *GetExtension(const char *name)
{
	const char *dot = strrchr(name, '.'), *slash;
	if (dot && (!(slash = strrchr(name, '/')) || slash < dot))
		return dot + 1;
	else
		return "";
}

void StripExtension(const char *in, char *out, int destsize)
{
	const char *dot = strrchr(in, '.'), *slash;

	if (dot && (!(slash = strrchr(in, '/')) || slash < dot))
		destsize = (destsize < dot - in + 1 ? destsize : dot - in + 1);

	if (in == out && destsize > 1)
		out[destsize - 1] = '\0';
	else
		Strncpyz(out, in, destsize);
}

int Sprintf(char *dest, int size, const char *fmt, ...)
{
	int		len;
	va_list		argptr;

	va_start(argptr, fmt);
	len = Vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);

	if (len >= size)
		ri.Printf(PRINT_WARNING, "util::Sprintf: Output length %d too short, require %d bytes.\n", size, len + 1);

	return len;
}

int Stricmpn(const char *s1, const char *s2, int n)
{
	int		c1, c2;

	if (s1 == NULL) {
		if (s2 == NULL)
			return 0;
		else
			return -1;
	}
	else if (s2 == NULL)
		return 1;



	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);

	return 0;		// strings are equal
}

int Stricmp(const char *s1, const char *s2)
{
	return (s1 && s2) ? Stricmpn(s1, s2, 99999) : -1;
}

void Strncpyz(char *dest, const char *src, int destsize)
{
	if (!dest) {
		ri.Error(ERR_FATAL, "util::Strncpyz: NULL dest");
	}
	if (!src) {
		ri.Error(ERR_FATAL, "util::Strncpyz: NULL src");
	}
	if (destsize < 1) {
		ri.Error(ERR_FATAL, "util::Strncpyz: destsize < 1");
	}

	strncpy(dest, src, destsize - 1);
	dest[destsize - 1] = 0;
}

void Strcat(char *dest, int size, const char *src)
{
	int		l1;

	l1 = strlen(dest);
	if (l1 >= size) {
		ri.Error(ERR_FATAL, "util::Strcat: already overflowed");
	}
	Strncpyz(dest + l1, src, size - l1);
}

char *ToLowerCase(char *s1)
{
	char	*s;

	s = s1;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return s1;
}

char *VarArgs(const char *format, ...)
{
	va_list		argptr;
	static char string[2][32000]; // in case va is called by nested functions
	static int	index = 0;
	char		*buf;

	buf = string[index & 1];
	index++;

	va_start(argptr, format);
	Vsnprintf(buf, sizeof(*string), format, argptr);
	va_end(argptr);

	return buf;
}

int Vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
#ifdef _MSC_VER
	int retval;

	retval = _vsnprintf(str, size, format, ap);

	if (retval < 0 || retval == size)
	{
		// Microsoft doesn't adhere to the C99 standard of vsnprintf,
		// which states that the return value must be the number of
		// bytes written if the output string had sufficient length.
		//
		// Obviously we cannot determine that value from Microsoft's
		// implementation, so we have no choice but to return size.

		str[size - 1] = '\0';
		return size;
	}

	return retval;
#else
	return vsnprintf(str, size, format, ap);
#endif
}

} // namespace util
} // namespace renderer
