/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// Filename:-	small_header.h
//
// This file is just used so I can isolate a few small structs from various horrible MP3 header files without
//	externalising code protos etc. This can now be included by both main game sound code (through sfx_t) and MP3 C code.
//

#ifndef SMALL_HEADER_H
#define SMALL_HEADER_H

typedef union {
    int s;
    float x;
} SAMPLE;

typedef struct {
    int in_bytes;
    int out_bytes;
} IN_OUT;

#ifdef WIN32 // Damn linux gcc isn't detecting byte as defined
#ifndef byte
typedef unsigned char byte;
#endif
#endif

#endif // #ifndef SMALL_HEADER_H

/////////////// eof ////////////
