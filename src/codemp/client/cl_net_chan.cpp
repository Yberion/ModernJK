/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

#include "client.h"

// TTimo: unused, commenting out to make gcc happy
#if 1
/*
==============
CL_Netchan_Encode

Removed with the client packets coded by netcodec: the client-to-server payload was
XORed with a key made of the challenge, the serverId, the acknowledge and the last command texts, read in clear at
the start of the packet, which the codec now codes.
==============
*/

/*
==============
CL_Netchan_Decode

Removed with the message codec: the server's payload was XORed with a key
chosen from the reliable acknowledge read in clear at the start of the message, which the codec now codes (see
SV_Netchan_Encode).
==============
*/
#endif

/*
=================
CL_Netchan_TransmitNextFragment
=================
*/
void CL_Netchan_TransmitNextFragment(netchan_t* chan)
{
    Netchan_TransmitNextFragment(chan);
}

// byte chksum[65536];

/*
===============
CL_Netchan_Transmit
================
*/
void CL_Netchan_Transmit(netchan_t* chan, msg_t* msg)
{
    // the message codec's bytes, as they are: no clc_EOF and no XOR (see CL_Netchan_Encode above)
    Netchan_Transmit(chan, msg->cursize, msg->data);
}

extern int oldsize;
int newsize = 0;

/*
=================
CL_Netchan_Process
=================
*/
bool CL_Netchan_Process(netchan_t* chan, msg_t* msg)
{
    int ret;
    //	int i;
    //	static		int newsize = 0;

    ret = Netchan_Process(chan, msg);
    if (!ret)
        return false;
    //	Huff_Decompress( msg, CL_DECODE_START );
    //	for(i=CL_DECODE_START+msg->readcount;i<msg->cursize;i++) {
    //		if (msg->data[i] != chksum[i-(CL_DECODE_START+msg->readcount)]) {
    //			Q_Error(errorParm_t::ERR_DROP, "bad {} v {}", msg->data[i], chksum[i-(CL_DECODE_START+msg->readcount)]);
    //		}
    //	}
    newsize += msg->cursize;
    //	Q_Println("saved {} to {} ({}%)", (oldsize>>3), newsize, 100-(newsize*100/(oldsize>>3)));
    return true;
}
