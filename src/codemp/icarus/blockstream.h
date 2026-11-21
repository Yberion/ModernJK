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

#pragma once

// BlockStream.h
#include "qcommon/qcommon.h"
#include <stdio.h>

#include <list>
#include <vector>

#include "tokens.h"

#define IBI_EXT ".IBI" //(I)nterpreted (B)lock (I)nstructions
#define IBI_HEADER_ID "IBI"
#include "qcommon/q_icarusflags.h"

const float IBI_VERSION = 1.57f;
const int MAX_FILENAME_LENGTH = 1024;

typedef float vector_t[3];

enum class pushPop_t { POP_FRONT, POP_BACK, PUSH_FRONT, PUSH_BACK };

// Templates

// CBlockMember

class CBlockMember {
public:
    CBlockMember();
    ~CBlockMember();

    void Free(void);

    int WriteMember(FILE*);       // Writes the member's data, in block format, to FILE *
    int ReadMember(char**, int*); // Reads the member's data, in block format, from FILE *

    void SetID(icarusToken_t id) { m_id = id; } // Set the ID member variable
    void SetSize(int size) { m_size = size; }   // Set the size member variable

    void GetInfo(icarusToken_t*, int*, void**);

    // SetData overloads
    void SetData(const char*);
    void SetData(vector_t);
    void SetData(void* data, int size);

    icarusToken_t GetID(void) const { return m_id; } // Get ID member variables
    void* GetData(void) const { return m_data; }     // Get data member variable
    int GetSize(void) const { return m_size; }       // Get size member variable

    inline void* operator new(size_t size)
    { // Allocate the memory.
        return Z_Malloc(size, memtag::TAG_ICARUS4, true);
    }
    // Overloaded delete operator.
    inline void operator delete(void* pRawData)
    { // Free the Memory.
        Z_Free(pRawData);
    }

    CBlockMember* Duplicate(void);

    template <class T> void WriteData(T& data)
    {
        if (m_data) {
            ICARUS_Free(m_data);
        }

        m_data = ICARUS_Malloc(sizeof(T));
        *((T*)m_data) = data;
        m_size = sizeof(T);
    }

    template <class T> void WriteDataPointer(const T* data, int num)
    {
        if (m_data) {
            ICARUS_Free(m_data);
        }

        m_data = ICARUS_Malloc(num * sizeof(T));
        memcpy(m_data, data, num * sizeof(T));
        m_size = num * sizeof(T);
    }

protected:
    icarusToken_t m_id; // ID of the value contained in data
    int m_size;         // Size of the data member variable
    void* m_data;       // Data for this member
};

// CBlock

#include "qcommon/q_icarusflags.h"

class CBlock {
    typedef std::vector<CBlockMember*> blockMember_v;

public:
    CBlock();
    ~CBlock();

    int Init(void);

    int Create(icarusToken_t);
    int Free();

    // Write Overloads

    int Write(icarusToken_t, vector_t);
    int Write(icarusToken_t, float);
    int Write(icarusToken_t, const char*);
    int Write(icarusToken_t, int);
    int Write(CBlockMember*);

    // Member push / pop functions

    int AddMember(CBlockMember*);
    CBlockMember* GetMember(int memberNum);

    void* GetMemberData(int memberNum);
    icarusToken_t GetMemberToken(int memberNum)
    {
        return static_cast<icarusToken_t>((int)*(float*)GetMemberData(memberNum));
    } // Get a member whose data is a token stored as a float

    CBlock* Duplicate(void);

    icarusToken_t GetBlockID(void) const { return m_id; }           // Get the ID for the block
    int GetNumMembers(void) const { return (int)m_members.size(); } // Get the number of member in the block's list

    void SetFlags(blockFlags_t flags) { m_flags = flags; }
    void SetFlag(blockFlags_t flag) { m_flags |= flag; }

    bool HasFlag(blockFlags_t flag) const { return Any(m_flags & flag); }
    blockFlags_t GetFlags(void) const { return m_flags; }

protected:
    blockMember_v m_members; // List of all CBlockMembers owned by this list
    icarusToken_t m_id;      // ID of the block
    blockFlags_t m_flags;
};

// CBlockStream

class CBlockStream {
public:
    CBlockStream();
    ~CBlockStream();

    int Init(void);

    int Create(char*);
    int Free(void);

    // Stream I/O functions

    int BlockAvailable(void);

    int WriteBlock(CBlock*); // Write the block out
    int ReadBlock(CBlock*);  // Read the block in

    int Open(char*, long); // Open a stream for reading / writing

protected:
    unsigned GetUnsignedInteger(void);
    int GetInteger(void);

    char GetChar(void);
    long GetLong(void);
    float GetFloat(void);

    long m_fileSize;                      // Size of the file
    FILE* m_fileHandle;                   // Global file handle of current I/O source
    char m_fileName[MAX_FILENAME_LENGTH]; // Name of the current file

    char* m_stream; // Stream of data to be parsed
    int m_streamPos;
};
