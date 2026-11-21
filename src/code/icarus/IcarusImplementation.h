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

// IcarusImplementation.h
#ifndef ICARUSIMPLEMENTATION_DEFINED
#define ICARUSIMPLEMENTATION_DEFINED

#ifndef ICARUSINTERFACE_DEFINED
#include "IcarusInterface.h"
#endif

#include <string>
#include <vector>
#include <map>
#include <list>
#include <algorithm>

class CSequence;
class CSequencer;

class CIcarusSequencer;
class CIcarusSequence;

class CIcarus : public IIcarusInterface {
public:
    CIcarus(int flavor);
    virtual ~CIcarus();

    inline IGameInterface* GetGame() { return IGameInterface::GetGame(m_flavor); };

    static constexpr int MAX_STRING_SIZE = 256;
    static constexpr int MAX_FILENAME_LENGTH = 1024;

protected:
    int m_flavor;
    int m_nextSequencerID;

    int m_GUID;

    typedef std::list<CSequence*> sequence_l;
    typedef std::list<CSequencer*> sequencer_l;
    typedef std::map<int, CSequencer*> sequencer_m;

    sequence_l m_sequences;
    sequencer_l m_sequencers;
    sequencer_m m_sequencerMap;

    typedef std::map<std::string, unsigned char> signal_m;
    signal_m m_signals;

    static double ICARUS_VERSION;

#ifdef _DEBUG

    int m_DEBUG_NumSequencerAlloc;
    int m_DEBUG_NumSequencerFreed;
    int m_DEBUG_NumSequencerResidual;

    int m_DEBUG_NumSequenceAlloc;
    int m_DEBUG_NumSequenceFreed;
    int m_DEBUG_NumSequenceResidual;

#endif

public:
    static int s_flavorsAvailable;
    static CIcarus** s_instances;

    // mandatory overrides
    // Get the current Game flavor.
    int GetFlavor();

    int Save();
    int Load();
    bool Run(int icarusID, char* buffer, long length);
    void DeleteIcarusID(int& icarusID);
    int GetIcarusID(int ownerID);
    int Update(int icarusID);

    int IsRunning(int icarusID);
    void Completed(int icarusID, int taskID);
    void Precache(char* buffer, long length);

protected:
    void Delete();
    void Free();

public:
    CSequence* GetSequence(int id);
    void DeleteSequence(CSequence* sequence);
    int AllocateSequences(int numSequences, int* idTable);
    CSequencer* FindSequencer(int sequencerID);
    CSequence* GetSequence();

protected:
    int SaveSequenceIDTable();
    int SaveSequences();
    int SaveSequencers();
    int SaveSignals();

    int LoadSignals();
    int LoadSequence();
    int LoadSequences();
    int LoadSequencers();

public:
    void Signal(const char* identifier);
    bool CheckSignal(const char* identifier);
    void ClearSignal(const char* identifier);

    // Overloaded new operator.
    inline void* operator new(size_t size) { return IGameInterface::GetGame()->Malloc(size); }

    // Overloaded delete operator.
    inline void operator delete(void* pRawData)
    {
        // Free the Memory.
        IGameInterface::GetGame()->Free(pRawData);
    }

public:
    // Used by the new Icarus Save code.
    static constexpr int MAX_BUFFER_SIZE = 100000;
    unsigned long m_ulBufferCurPos;
    unsigned long m_ulBytesRead;
    unsigned char* m_byBuffer;
    // Destroy the File Buffer.
    void DestroyBuffer();
    // Create the File Buffer.
    void CreateBuffer();
    // Reset the buffer completely.
    void ResetBuffer();
    // Write to a buffer.
    void BufferWrite(void* pSrcData, unsigned long ulNumBytesToWrite);
    // Read from a buffer.
    void BufferRead(void* pDstBuff, unsigned long ulNumBytesToRead);
};

#endif
