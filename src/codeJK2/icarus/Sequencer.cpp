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

// Script Command Sequencer
//
//	-- jweier

// this include must remain at the top of every Icarus CPP file
#include "icarus.h"
#include "g_headers.h"
#include "g_shared.h"

#include "assert.h"
#include "../code/qcommon/ojk_saved_game_helper.h"

// Sequencer

CSequencer::CSequencer(void)
{
    m_numCommands = 0;

    m_curStream = nullptr;
    m_curSequence = nullptr;

    m_elseValid = 0;
    m_elseOwner = nullptr;

    m_curGroup = nullptr;
}

CSequencer::~CSequencer(void)
{
    Free(); // Safe even if already freed
}

/*
========================
Create

Static creation function
========================
*/

CSequencer* CSequencer::Create(void)
{
    CSequencer* sequencer = new CSequencer;

    return sequencer;
}

/*
========================
Init

Initializes the sequencer
========================
*/
sequencerResult_t CSequencer::Init(int ownerID, interface_export_t* ie, CTaskManager* taskManager, ICARUS_Instance* iICARUS)
{
    m_ownerID = ownerID;
    m_owner = iICARUS;
    m_taskManager = taskManager;
    m_ie = ie;

    return sequencerResult_t::SEQ_OK;
}

/*
========================
Free

Releases all resources and re-inits the sequencer
========================
*/
sequencerResult_t CSequencer::Free(void)
{
    sequence_l::iterator sli;

    // Flush the sequences
    for (sli = m_sequences.begin(); sli != m_sequences.end(); ++sli) {
        m_owner->DeleteSequence((*sli));
    }

    m_sequences.clear();
    m_sequenceMap.clear();
    m_taskSequences.clear();

    // Clean up any other info
    m_numCommands = 0;
    m_curSequence = nullptr;

    bstream_t* streamToDel;
    while (!m_streamsCreated.empty()) {
        streamToDel = m_streamsCreated.back();
        DeleteStream(streamToDel);
    }

    return sequencerResult_t::SEQ_OK;
}

/*
-------------------------
Flush
-------------------------
*/

sequencerResult_t CSequencer::Flush(CSequence* owner)
{
    if (owner == nullptr)
        return sequencerResult_t::SEQ_FAILED;

    Recall();

    sequence_l::iterator sli;

    // Flush the sequences
    for (sli = m_sequences.begin(); sli != m_sequences.end();) {
        if (((*sli) == owner) || (owner->HasChild((*sli))) || ((*sli)->HasFlag(sequenceFlags_t::SQ_PENDING)) || ((*sli)->HasFlag(sequenceFlags_t::SQ_TASK))) {
            ++sli;
            continue;
        }

        // Remove it from the map
        m_sequenceMap.erase((*sli)->GetID());

        // Delete it, and remove all references
        RemoveSequence((*sli));
        m_owner->DeleteSequence((*sli));

        // Delete from the sequence list and move on
        sli = m_sequences.erase(sli);
    }

    // Make sure this owner knows it's now the root sequence
    owner->SetParent(nullptr);
    owner->SetReturn(nullptr);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
AddStream

Creates a stream for parsing
========================
*/

bstream_t* CSequencer::AddStream(void)
{
    bstream_t* stream;

    stream = new bstream_t;            // deleted in Route()
    stream->stream = new CBlockStream; // deleted in Route()
    stream->last = m_curStream;

    m_streamsCreated.push_back(stream);

    return stream;
}

/*
========================
DeleteStream

Deletes parsing stream
========================
*/
void CSequencer::DeleteStream(bstream_t* bstream)
{
    std::vector<bstream_t*>::iterator finder = std::find(m_streamsCreated.begin(), m_streamsCreated.end(), bstream);
    if (finder != m_streamsCreated.end()) {
        m_streamsCreated.erase(finder);
    }

    bstream->stream->Free();

    delete bstream->stream;
    delete bstream;

    bstream = nullptr;
}

/*
-------------------------
AddTaskSequence
-------------------------
*/

void CSequencer::AddTaskSequence(CSequence* sequence, CTaskGroup* group)
{
    m_taskSequences[group] = sequence;
}

/*
-------------------------
GetTaskSequence
-------------------------
*/

CSequence* CSequencer::GetTaskSequence(CTaskGroup* group)
{
    taskSequence_m::iterator tsi;

    tsi = m_taskSequences.find(group);

    if (tsi == m_taskSequences.end())
        return nullptr;

    return (*tsi).second;
}

/*
========================
AddSequence

Creates and adds a sequence to the sequencer
========================
*/

CSequence* CSequencer::AddSequence(void)
{
    CSequence* sequence = m_owner->GetSequence();

    assert(sequence);
    if (sequence == nullptr)
        return nullptr;

    // The rest is handled internally to the class
    m_sequenceMap[sequence->GetID()] = sequence;

    // Add it to the list
    m_sequences.insert(m_sequences.end(), sequence);

    // FIXME: Temp fix
    sequence->SetFlag(sequenceFlags_t::SQ_PENDING);

    return sequence;
}

CSequence* CSequencer::AddSequence(CSequence* parent, CSequence* returnSeq, sequenceFlags_t flags)
{
    CSequence* sequence = m_owner->GetSequence();

    assert(sequence);
    if (sequence == nullptr)
        return nullptr;

    // The rest is handled internally to the class
    m_sequenceMap[sequence->GetID()] = sequence;

    // Add it to the list
    m_sequences.insert(m_sequences.end(), sequence);

    sequence->SetFlags(flags);
    sequence->SetParent(parent);
    sequence->SetReturn(returnSeq);

    return sequence;
}

/*
========================
GetSequence

Retrieves a sequence by its ID
========================
*/

CSequence* CSequencer::GetSequence(int id)
{
    sequenceID_m::iterator mi;

    mi = m_sequenceMap.find(id);

    if (mi == m_sequenceMap.end())
        return nullptr;

    return (*mi).second;
}

/*
-------------------------
Interrupt
-------------------------
*/

void CSequencer::Interrupt(void)
{
    CBlock* command = m_taskManager->GetCurrentTask();

    if (command == nullptr)
        return;

    // Save it
    PushCommand(command, pushPop_t::PUSH_BACK);
}

/*
========================
Run

Runs a script
========================
*/
sequencerResult_t CSequencer::Run(char* buffer, long size)
{
    bstream_t* blockStream;

    Recall();

    // Create a new stream
    blockStream = AddStream();

    // Open the stream as an IBI stream
    if (!blockStream->stream->Open(buffer, size)) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "invalid stream");
        return sequencerResult_t::SEQ_FAILED;
    }

    CSequence* sequence = AddSequence(nullptr, m_curSequence, sequenceFlags_t::SQ_COMMON);

    // Interpret the command blocks and route them properly
    if (S_FAILED(Route(sequence, blockStream))) {
        // Error code is set inside of Route()
        return sequencerResult_t::SEQ_FAILED;
    }

    return sequencerResult_t::SEQ_OK;
}

/*
========================
ParseRun

Parses a user triggered run command
========================
*/

sequencerResult_t CSequencer::ParseRun(CBlock* block)
{
    CSequence* new_sequence;
    bstream_t* new_stream;
    char* buffer;
    char newname[MAX_STRING_SIZE];
    int buffer_size;

    // Get the name and format it
    COM_StripExtension((char*)block->GetMemberData(0), (char*)newname, sizeof(newname));

    // Get the file from the game engine
    buffer_size = m_ie->I_LoadFile(newname, (void**)&buffer);

    if (buffer_size <= 0) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "'%s' : could not open file\n", (char*)block->GetMemberData(0));
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    // Create a new stream for this file
    new_stream = AddStream();

    // Begin streaming the file
    if (!new_stream->stream->Open(buffer, buffer_size)) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "invalid stream");
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    // Create a new sequence
    new_sequence = AddSequence(m_curSequence, m_curSequence, (sequenceFlags_t::SQ_RUN | sequenceFlags_t::SQ_PENDING));

    m_curSequence->AddChild(new_sequence);

    // Interpret the command blocks and route them properly
    if (S_FAILED(Route(new_sequence, new_stream))) {
        // Error code is set inside of Route()
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    m_curSequence = m_curSequence->GetReturn();

    assert(m_curSequence);

    block->Write(icarusToken_t::TK_FLOAT, (float)new_sequence->GetID());
    PushCommand(block, pushPop_t::PUSH_FRONT);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
ParseIf

Parses an if statement
========================
*/

sequencerResult_t CSequencer::ParseIf(CBlock* block, bstream_t* bstream)
{
    CSequence* sequence;

    // Create the container sequence
    sequence = AddSequence(m_curSequence, m_curSequence, sequenceFlags_t::SQ_CONDITIONAL);

    assert(sequence);
    if (sequence == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "ParseIf: failed to allocate container sequence");
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    m_curSequence->AddChild(sequence);

    // Add a unique conditional identifier to the block for reference later
    block->Write(icarusToken_t::TK_FLOAT, (float)sequence->GetID());

    // Push this onto the stack to mark the conditional entrance
    PushCommand(block, pushPop_t::PUSH_FRONT);

    // Recursively obtain the conditional body
    Route(sequence, bstream);

    m_elseValid = 2;
    m_elseOwner = block;

    return sequencerResult_t::SEQ_OK;
}

/*
========================
ParseElse

Parses an else statement
========================
*/

sequencerResult_t CSequencer::ParseElse(CBlock* block, bstream_t* bstream)
{
    // The else is not retained
    delete block;
    block = nullptr;

    CSequence* sequence;

    // Create the container sequence
    sequence = AddSequence(m_curSequence, m_curSequence, sequenceFlags_t::SQ_CONDITIONAL);

    assert(sequence);
    if (sequence == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "ParseIf: failed to allocate container sequence");
        return sequencerResult_t::SEQ_FAILED;
    }

    m_curSequence->AddChild(sequence);

    // Add a unique conditional identifier to the block for reference later
    // TODO: Emit warning
    if (m_elseOwner == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid 'else' found!\n");
        return sequencerResult_t::SEQ_FAILED;
    }

    m_elseOwner->Write(icarusToken_t::TK_FLOAT, (float)sequence->GetID());

    m_elseOwner->SetFlag(blockFlags_t::BF_ELSE);

    // Recursively obtain the conditional body
    Route(sequence, bstream);

    m_elseValid = 0;
    m_elseOwner = nullptr;

    return sequencerResult_t::SEQ_OK;
}

/*
========================
ParseLoop

Parses a loop command
========================
*/

sequencerResult_t CSequencer::ParseLoop(CBlock* block, bstream_t* bstream)
{
    CSequence* sequence;
    CBlockMember* bm;
    float min, max;
    int rIter;
    int memberNum = 0;

    // Set the parent
    sequence = AddSequence(m_curSequence, m_curSequence, (sequenceFlags_t::SQ_LOOP | sequenceFlags_t::SQ_RETAIN));

    assert(sequence);
    if (sequence == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "ParseLoop : failed to allocate container sequence");
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    m_curSequence->AddChild(sequence);

    // Set the number of iterations of this sequence

    bm = block->GetMember(memberNum++);

    if (bm->GetID() == icarusToken_t::ID_RANDOM) {
        // Parse out the random number
        min = *(float*)block->GetMemberData(memberNum++);
        max = *(float*)block->GetMemberData(memberNum++);

        rIter = (int)m_ie->I_Random(min, max);
        sequence->SetIterations(rIter);
    }
    else {
        sequence->SetIterations((int)(*(float*)bm->GetData()));
    }

    // Add a unique loop identifier to the block for reference later
    block->Write(icarusToken_t::TK_FLOAT, (float)sequence->GetID());

    // Push this onto the stack to mark the loop entrance
    PushCommand(block, pushPop_t::PUSH_FRONT);

    // Recursively obtain the loop
    Route(sequence, bstream);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
AddAffect

Adds a sequence that is saved until the affect is called by the parent
========================
*/

sequencerResult_t CSequencer::AddAffect(bstream_t* bstream, int retain, int* id)
{
    CSequence* sequence = AddSequence();
    bstream_t new_stream;

    sequence->SetFlag(sequenceFlags_t::SQ_AFFECT | sequenceFlags_t::SQ_PENDING);

    if (retain)
        sequence->SetFlag(sequenceFlags_t::SQ_RETAIN);

    // This will be replaced once it's actually used, but this will restore the route state properly
    sequence->SetReturn(m_curSequence);

    // We need this as a temp holder
    new_stream.last = m_curStream;
    new_stream.stream = bstream->stream;

    if S_FAILED (Route(sequence, &new_stream)) {
        return sequencerResult_t::SEQ_FAILED;
    }

    *id = sequence->GetID();

    sequence->SetReturn(nullptr);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
ParseAffect

Parses an affect command
========================
*/

sequencerResult_t CSequencer::ParseAffect(CBlock* block, bstream_t* bstream)
{
    CSequencer* stream_sequencer = nullptr;
    char* entname = nullptr;
    int ret;
    gentity_t* ent = nullptr;

    entname = (char*)block->GetMemberData(0);
    ent = m_ie->I_GetEntityByName(entname);

    if (!ent) // if there wasn't a valid entname in the affect, we need to check if it's a get command
    {
        // try to parse a 'get' command that is embeded in this 'affect'

        icarusToken_t id;
        const char* p1 = nullptr;
        char* name = nullptr;
        CBlockMember* bm = nullptr;
        //
        //	Get the first parameter (this should be the get)
        //
        bm = block->GetMember(0);
        id = bm->GetID();

        switch (id) {
        // these 3 cases probably aren't necessary
        case icarusToken_t::TK_STRING:
        case icarusToken_t::TK_IDENTIFIER:
        case icarusToken_t::TK_CHAR:
            p1 = (char*)bm->GetData();
            break;

        case icarusToken_t::ID_GET: {
            icarusToken_t type;

            // get( TYPE, NAME )
            type = block->GetMemberToken(1);
            name = (char*)block->GetMemberData(2);

            switch (type) // what type are they attempting to get
            {

            case icarusToken_t::TK_STRING:
                // only string is acceptable for affect, store result in p1
                if (m_ie->I_GetString(m_ownerID, type, name, &p1) == false) {
                    delete block;
                    block = nullptr;
                    return sequencerResult_t::SEQ_OK;
                }
                break;
            default:
                // FIXME: Make an enum id for the error...
                m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on affect _1");
                delete block;
                block = nullptr;
                return sequencerResult_t::SEQ_OK;
                break;
            }

            break;
        }

        default:
            // FIXME: Make an enum id for the error...
            m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on affect _2");
            delete block;
            block = nullptr;
            return sequencerResult_t::SEQ_OK;
            break;
        } // end id switch

        if (p1) {
            ent = m_ie->I_GetEntityByName(p1);
        }
        if (!ent) { // a valid entity name was not returned from the get command
            m_ie->I_DPrintf(WL_e::WL_WARNING, "'%s' : invalid affect() target\n", p1 ? p1 : "");
        }

    } // end if(!ent)

    if (ent) {
        stream_sequencer = ent->sequencer;
    }

    if (stream_sequencer == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_WARNING, "'%s' : invalid affect() target\n", entname);

        // Fast-forward out of this affect block onto the next valid code
        CSequence* backSeq = m_curSequence;

        CSequence* trashSeq = m_owner->GetSequence();
        Route(trashSeq, bstream);
        Recall();
        DestroySequence(trashSeq);
        m_curSequence = backSeq;
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_OK;
    }

    if S_FAILED (stream_sequencer->AddAffect(bstream, (int)m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN), &ret)) {
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    // Hold onto the id for later use
    // FIXME: If the target sequence is freed, what then?		(!suspect!)

    block->Write(icarusToken_t::TK_FLOAT, (float)ret);

    PushCommand(block, pushPop_t::PUSH_FRONT);
    /*
    //Don't actually do these right now, we're just pre-processing (parsing) the affect
    if( ent )
    {	// ents need to update upon being affected
        ent->taskManager->Update();
    }
    */

    return sequencerResult_t::SEQ_OK;
}

/*
-------------------------
ParseTask
-------------------------
*/

sequencerResult_t CSequencer::ParseTask(CBlock* block, bstream_t* bstream)
{
    CSequence* sequence;
    CTaskGroup* group;
    const char* taskName;

    // Setup the container sequence
    sequence = AddSequence(m_curSequence, m_curSequence, sequenceFlags_t::SQ_TASK | sequenceFlags_t::SQ_RETAIN);
    m_curSequence->AddChild(sequence);

    // Get the name of this task for reference later
    taskName = (const char*)block->GetMemberData(0);

    // Get a new task group from the task manager
    group = m_taskManager->AddTaskGroup(taskName);

    if (group == nullptr) {
        m_ie->I_DPrintf(WL_e::WL_ERROR, "error : unable to allocate a new task group");
        delete block;
        block = nullptr;
        return sequencerResult_t::SEQ_FAILED;
    }

    // The current group is set to this group, all subsequent commands (until a block end) will fall into this task group
    group->SetParent(m_curGroup);
    m_curGroup = group;

    // Keep an association between this task and the container sequence
    AddTaskSequence(sequence, group);

    // PushCommand( block, pushPop_t::PUSH_FRONT );
    delete block;
    block = nullptr;

    // Recursively obtain the loop
    Route(sequence, bstream);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
Route

Properly handles and routes commands to the sequencer
========================
*/

// FIXME: Re-entering this code will produce unpredictable results if a script has already been routed and is running currently

// FIXME: A sequencer cannot properly affect itself

sequencerResult_t CSequencer::Route(CSequence* sequence, bstream_t* bstream)
{
    CBlockStream* stream;
    CBlock* block;

    // Take the stream as the current stream
    m_curStream = bstream;
    stream = bstream->stream;

    m_curSequence = sequence;

    // Obtain all blocks
    while (stream->BlockAvailable()) {
        block = new CBlock; // deleted in Free()
        stream->ReadBlock(block);

        // TEMP: HACK!
        if (m_elseValid)
            m_elseValid--;

        switch (block->GetBlockID()) {
        // Marks the end of a blocked section
        case icarusToken_t::ID_BLOCK_END:

            // Save this as a pre-process marker
            PushCommand(block, pushPop_t::PUSH_FRONT);

            if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RUN) || m_curSequence->HasFlag(sequenceFlags_t::SQ_AFFECT)) {
                // Go back to the last stream
                m_curStream = bstream->last;
            }

            if (m_curSequence->HasFlag(sequenceFlags_t::SQ_TASK)) {
                // Go back to the last stream
                m_curStream = bstream->last;
                m_curGroup = m_curGroup->GetParent();
            }

            m_curSequence = m_curSequence->GetReturn();

            return sequencerResult_t::SEQ_OK;
            break;

        // Affect pre-processor
        case icarusToken_t::ID_AFFECT:

            if S_FAILED (ParseAffect(block, bstream))
                return sequencerResult_t::SEQ_FAILED;

            break;

        // Run pre-processor
        case icarusToken_t::ID_RUN:

            if S_FAILED (ParseRun(block))
                return sequencerResult_t::SEQ_FAILED;

            break;

        // Loop pre-processor
        case icarusToken_t::ID_LOOP:

            if S_FAILED (ParseLoop(block, bstream))
                return sequencerResult_t::SEQ_FAILED;

            break;

        // Conditional pre-processor
        case icarusToken_t::ID_IF:

            if S_FAILED (ParseIf(block, bstream))
                return sequencerResult_t::SEQ_FAILED;

            break;

        case icarusToken_t::ID_ELSE:

            // TODO: Emit warning
            if (m_elseValid == 0) {
                m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid 'else' found!\n");
                return sequencerResult_t::SEQ_FAILED;
            }

            if S_FAILED (ParseElse(block, bstream))
                return sequencerResult_t::SEQ_FAILED;

            break;

        case icarusToken_t::ID_TASK:

            if S_FAILED (ParseTask(block, bstream))
                return sequencerResult_t::SEQ_FAILED;

            break;

        // FIXME: For now this is to catch problems, but can ultimately be removed
        case icarusToken_t::ID_WAIT:
        case icarusToken_t::ID_PRINT:
        case icarusToken_t::ID_SOUND:
        case icarusToken_t::ID_MOVE:
        case icarusToken_t::ID_ROTATE:
        case icarusToken_t::ID_SET:
        case icarusToken_t::ID_USE:
        case icarusToken_t::ID_REMOVE:
        case icarusToken_t::ID_KILL:
        case icarusToken_t::ID_FLUSH:
        case icarusToken_t::ID_CAMERA:
        case icarusToken_t::ID_DO:
        case icarusToken_t::ID_DECLARE:
        case icarusToken_t::ID_FREE:
        case icarusToken_t::ID_SIGNAL:
        case icarusToken_t::ID_WAITSIGNAL:
        case icarusToken_t::ID_PLAY:

            // Commands go directly into the sequence without pre-process
            PushCommand(block, pushPop_t::PUSH_FRONT);
            break;

        // Error
        default:

            m_ie->I_DPrintf(WL_e::WL_ERROR, "'%d' : invalid block ID", to_underlying(block->GetBlockID()));

            return sequencerResult_t::SEQ_FAILED;
            break;
        }
    }

    // Check for a run sequence, it must be marked
    if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RUN)) {
        block = new CBlock;
        block->Create(icarusToken_t::ID_BLOCK_END);
        PushCommand(block, pushPop_t::PUSH_FRONT); // mark the end of the run

        /*
        //Free the stream
        m_curStream = bstream->last;
        DeleteStream( bstream );
        */

        return sequencerResult_t::SEQ_OK;
    }

    // Check to start the communication
    if ((bstream->last == nullptr) && (m_numCommands > 0)) {
        // Everything is routed, so get it all rolling
        Prime(m_taskManager, PopCommand(pushPop_t::POP_BACK));
    }

    m_curStream = bstream->last;

    // Free the stream
    DeleteStream(bstream);

    return sequencerResult_t::SEQ_OK;
}

/*
========================
CheckRun

Checks for run command pre-processing
========================
*/

// Directly changes the parameter to avoid excess push/pop

void CSequencer::CheckRun(CBlock** command)
{
    CBlock* block = *command;

    if (block == nullptr)
        return;

    // Check for a run command
    if (block->GetBlockID() == icarusToken_t::ID_RUN) {
        int id = (int)(*(float*)block->GetMemberData(1));

        m_ie->I_DPrintf(WL_e::WL_DEBUG, "%4d run( \"%s\" ); [%d]", m_ownerID, (char*)block->GetMemberData(0), m_ie->I_GetTime());

        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;

            *command = nullptr;
        }

        m_curSequence = GetSequence(id);

        // TODO: Emit warning
        assert(m_curSequence);
        if (m_curSequence == nullptr) {
            m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find 'run' sequence!\n");
            *command = nullptr;
            return;
        }

        if (m_curSequence->GetNumCommands() > 0) {
            *command = PopCommand(pushPop_t::POP_BACK);

            Prep(command); // Account for any other pre-processes
            return;
        }

        return;
    }

    // Check for the end of a run
    if ((block->GetBlockID() == icarusToken_t::ID_BLOCK_END) && (m_curSequence->HasFlag(sequenceFlags_t::SQ_RUN))) {
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        m_curSequence = ReturnSequence(m_curSequence);

        if (m_curSequence && m_curSequence->GetNumCommands() > 0) {
            *command = PopCommand(pushPop_t::POP_BACK);

            Prep(command); // Account for any other pre-processes
            return;
        }

        // FIXME: Check this...
    }
}

/*
-------------------------
EvaluateConditional
-------------------------
*/

// FIXME: This function will be written better later once the functionality of the ideas here are tested

int CSequencer::EvaluateConditional(CBlock* block)
{
    CBlockMember* bm;
    char tempString1[128], tempString2[128];
    vector_t vec;
    icarusToken_t id, oper;
    int i, memberNum = 0;
    const char *p1 = nullptr, *p2 = nullptr;
    icarusToken_t t1, t2;

    //
    //	Get the first parameter
    //

    bm = block->GetMember(memberNum++);
    id = bm->GetID();

    t1 = id;

    switch (id) {
    case icarusToken_t::TK_FLOAT:
        Com_sprintf(tempString1, sizeof(tempString1), "%.3f", *(float*)bm->GetData());
        p1 = (char*)tempString1;
        break;

    case icarusToken_t::TK_VECTOR:

        tempString1[0] = '\0';

        for (i = 0; i < 3; i++) {
            bm = block->GetMember(memberNum++);
            vec[i] = *(float*)bm->GetData();
        }

        Com_sprintf(tempString1, sizeof(tempString1), "%.3f %.3f %.3f", vec[0], vec[1], vec[2]);
        p1 = (char*)tempString1;

        break;

    case icarusToken_t::TK_STRING:
    case icarusToken_t::TK_IDENTIFIER:
    case icarusToken_t::TK_CHAR:

        p1 = (char*)bm->GetData();
        break;

    case icarusToken_t::ID_GET: {
        icarusToken_t type;
        char* name;

        // get( TYPE, NAME )
        type = block->GetMemberToken(memberNum++);
        name = (char*)block->GetMemberData(memberNum++);

        // Get the type returned and hold onto it
        t1 = type;

        switch (type) {
        case icarusToken_t::TK_FLOAT: {
            float fVal;

            if (m_ie->I_GetFloat(m_ownerID, type, name, &fVal) == false)
                return false;

            Com_sprintf(tempString1, sizeof(tempString1), "%.3f", fVal);
            p1 = (char*)tempString1;
        }

        break;

        case icarusToken_t::TK_INT: {
            float fVal;

            if (m_ie->I_GetFloat(m_ownerID, type, name, &fVal) == false)
                return false;

            Com_sprintf(tempString1, sizeof(tempString1), "%d", (int)fVal);
            p1 = (char*)tempString1;
        } break;

        case icarusToken_t::TK_STRING:

            if (m_ie->I_GetString(m_ownerID, type, name, &p1) == false)
                return false;

            break;

        case icarusToken_t::TK_VECTOR: {
            vector_t vVal;

            if (m_ie->I_GetVector(m_ownerID, type, name, vVal) == false)
                return false;

            Com_sprintf(tempString1, sizeof(tempString1), "%.3f %.3f %.3f", vVal[0], vVal[1], vVal[2]);
            p1 = (char*)tempString1;
        }

        break;

        default:
            break;
        }

        break;
    }

    case icarusToken_t::ID_RANDOM: {
        float min, max;
        // FIXME: This will not account for nested Q_flrand(0.0f, 1.0f) statements

        min = *(float*)block->GetMemberData(memberNum++);
        max = *(float*)block->GetMemberData(memberNum++);

        // A float value is returned from the function
        t1 = icarusToken_t::TK_FLOAT;

        Com_sprintf(tempString1, sizeof(tempString1), "%.3f", m_ie->I_Random(min, max));
        p1 = (char*)tempString1;
    }

    break;

    case icarusToken_t::ID_TAG: {
        char* name;
        icarusToken_t type;

        name = (char*)block->GetMemberData(memberNum++);
        type = block->GetMemberToken(memberNum++);

        t1 = icarusToken_t::TK_VECTOR;

        // TODO: Emit warning
        if (m_ie->I_GetTag(m_ownerID, name, type, vec) == false) {
            m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find tag \"%s\"!\n", name);
            return false;
        }

        Com_sprintf(tempString1, sizeof(tempString1), "%.3f %.3f %.3f", vec[0], vec[1], vec[2]);
        p1 = (char*)tempString1;

        break;
    }

    default:
        // FIXME: Make an enum id for the error...
        m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on conditional");
        return false;
        break;
    }

    //
    //	Get the comparison operator
    //

    bm = block->GetMember(memberNum++);
    id = bm->GetID();

    switch (id) {
    case icarusToken_t::TK_EQUALS:
    case icarusToken_t::TK_GREATER_THAN:
    case icarusToken_t::TK_LESS_THAN:
    case icarusToken_t::TK_NOT:
        oper = id;
        break;

    default:
        m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid operator type found on conditional!\n");
        return false; // FIXME: Emit warning
        break;
    }

    //
    //	Get the second parameter
    //

    bm = block->GetMember(memberNum++);
    id = bm->GetID();

    t2 = id;

    switch (id) {
    case icarusToken_t::TK_FLOAT:
        Com_sprintf(tempString2, sizeof(tempString2), "%.3f", *(float*)bm->GetData());
        p2 = (char*)tempString2;
        break;

    case icarusToken_t::TK_VECTOR:

        tempString2[0] = '\0';

        for (i = 0; i < 3; i++) {
            bm = block->GetMember(memberNum++);
            vec[i] = *(float*)bm->GetData();
        }

        Com_sprintf(tempString2, sizeof(tempString2), "%.3f %.3f %.3f", vec[0], vec[1], vec[2]);
        p2 = (char*)tempString2;

        break;

    case icarusToken_t::TK_STRING:
    case icarusToken_t::TK_IDENTIFIER:
    case icarusToken_t::TK_CHAR:

        p2 = (char*)bm->GetData();
        break;

    case icarusToken_t::ID_GET: {
        icarusToken_t type;
        char* name;

        // get( TYPE, NAME )
        type = block->GetMemberToken(memberNum++);
        name = (char*)block->GetMemberData(memberNum++);

        // Get the type returned and hold onto it
        t2 = type;

        switch (type) {
        case icarusToken_t::TK_FLOAT: {
            float fVal;

            if (m_ie->I_GetFloat(m_ownerID, type, name, &fVal) == false)
                return false;

            Com_sprintf(tempString2, sizeof(tempString2), "%.3f", fVal);
            p2 = (char*)tempString2;
        }

        break;

        case icarusToken_t::TK_INT: {
            float fVal;

            if (m_ie->I_GetFloat(m_ownerID, type, name, &fVal) == false)
                return false;

            Com_sprintf(tempString2, sizeof(tempString2), "%d", (int)fVal);
            p2 = (char*)tempString2;
        } break;

        case icarusToken_t::TK_STRING:

            if (m_ie->I_GetString(m_ownerID, type, name, &p2) == false)
                return false;

            break;

        case icarusToken_t::TK_VECTOR: {
            vector_t vVal;

            if (m_ie->I_GetVector(m_ownerID, type, name, vVal) == false)
                return false;

            Com_sprintf(tempString2, sizeof(tempString2), "%.3f %.3f %.3f", vVal[0], vVal[1], vVal[2]);
            p2 = (char*)tempString2;
        }

        break;

        default:
            break;
        }

        break;
    }

    case icarusToken_t::ID_RANDOM:

    {
        float min, max;
        // FIXME: This will not account for nested Q_flrand(0.0f, 1.0f) statements

        min = *(float*)block->GetMemberData(memberNum++);
        max = *(float*)block->GetMemberData(memberNum++);

        // A float value is returned from the function
        t2 = icarusToken_t::TK_FLOAT;

        Com_sprintf(tempString2, sizeof(tempString2), "%.3f", m_ie->I_Random(min, max));
        p2 = (char*)tempString2;
    }

    break;

    case icarusToken_t::ID_TAG:

    {
        char* name;
        icarusToken_t type;

        name = (char*)block->GetMemberData(memberNum++);
        type = block->GetMemberToken(memberNum++);

        t2 = icarusToken_t::TK_VECTOR;

        // TODO: Emit warning
        if (m_ie->I_GetTag(m_ownerID, name, type, vec) == false) {
            m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find tag \"%s\"!\n", name);
            return false;
        }

        Com_sprintf(tempString2, sizeof(tempString2), "%.3f %.3f %.3f", vec[0], vec[1], vec[2]);
        p2 = (char*)tempString2;

        break;
    }

    default:
        // FIXME: Make an enum id for the error...
        m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on conditional");
        return false;
        break;
    }

    return m_ie->I_Evaluate(t1, p1, t2, p2, oper);
}

/*
========================
CheckIf

Checks for if statement pre-processing
========================
*/

void CSequencer::CheckIf(CBlock** command)
{
    CBlock* block = *command;
    int successID, failureID;
    CSequence *successSeq, *failureSeq;

    if (block == nullptr)
        return;

    if (block->GetBlockID() == icarusToken_t::ID_IF) {
        int ret = EvaluateConditional(block);

        if (ret /*TRUE*/) {
            if (block->HasFlag(blockFlags_t::BF_ELSE)) {
                successID = (int)(*(float*)block->GetMemberData(block->GetNumMembers() - 2));
            }
            else {
                successID = (int)(*(float*)block->GetMemberData(block->GetNumMembers() - 1));
            }

            successSeq = GetSequence(successID);

            // TODO: Emit warning
            assert(successSeq);
            if (successSeq == nullptr) {
                m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find conditional success sequence!\n");
                *command = nullptr;
                return;
            }

            // Only save the conditional statement if the calling sequence is retained
            if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
                PushCommand(block, pushPop_t::PUSH_FRONT);
            }
            else {
                delete block;
                block = nullptr;
                *command = nullptr;
            }

            m_curSequence = successSeq;

            // Recursively work out any other pre-processors
            *command = PopCommand(pushPop_t::POP_BACK);
            Prep(command);

            return;
        }

        if ((ret == false) && (block->HasFlag(blockFlags_t::BF_ELSE))) {
            failureID = (int)(*(float*)block->GetMemberData(block->GetNumMembers() - 1));
            failureSeq = GetSequence(failureID);

            // TODO: Emit warning
            assert(failureSeq);
            if (failureSeq == nullptr) {
                m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find conditional failure sequence!\n");
                *command = nullptr;
                return;
            }

            // Only save the conditional statement if the calling sequence is retained
            if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
                PushCommand(block, pushPop_t::PUSH_FRONT);
            }
            else {
                delete block;
                block = nullptr;
                *command = nullptr;
            }

            m_curSequence = failureSeq;

            // Recursively work out any other pre-processors
            *command = PopCommand(pushPop_t::POP_BACK);
            Prep(command);

            return;
        }

        // Only save the conditional statement if the calling sequence is retained
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        // Conditional failed, just move on to the next command
        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);

        return;
    }

    if ((block->GetBlockID() == icarusToken_t::ID_BLOCK_END) && (m_curSequence->HasFlag(sequenceFlags_t::SQ_CONDITIONAL))) {
        assert(m_curSequence->GetReturn());
        if (m_curSequence->GetReturn() == nullptr) {
            *command = nullptr;
            return;
        }

        // Check to retain it
        if (m_curSequence->GetParent()->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        // Back out of the conditional and resume the previous sequence
        m_curSequence = ReturnSequence(m_curSequence);

        // This can safely happen
        if (m_curSequence == nullptr) {
            *command = nullptr;
            return;
        }

        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);
    }
}

/*
========================
CheckLoop

Checks for loop command pre-processing
========================
*/

void CSequencer::CheckLoop(CBlock** command)
{
    CBlockMember* bm;
    CBlock* block = *command;
    float min, max;
    int iterations;
    int loopID;
    int memberNum = 0;

    if (block == nullptr)
        return;

    // Check for a loop
    if (block->GetBlockID() == icarusToken_t::ID_LOOP) {
        // Get the loop ID
        bm = block->GetMember(memberNum++);

        if (bm->GetID() == icarusToken_t::ID_RANDOM) {
            // Parse out the random number
            min = *(float*)block->GetMemberData(memberNum++);
            max = *(float*)block->GetMemberData(memberNum++);

            iterations = (int)m_ie->I_Random(min, max);
        }
        else {
            iterations = (int)(*(float*)bm->GetData());
        }

        loopID = (int)(*(float*)block->GetMemberData(memberNum++));

        CSequence* loop = GetSequence(loopID);

        // TODO: Emit warning
        assert(loop);
        if (loop == nullptr) {
            m_ie->I_DPrintf(WL_e::WL_ERROR, "Unable to find 'loop' sequence!\n");
            *command = nullptr;
            return;
        }

        assert(loop->GetParent());
        if (loop->GetParent() == nullptr) {
            *command = nullptr;
            return;
        }

        // Restore the count if it has been lost
        loop->SetIterations(iterations);

        // Only save the loop command if the calling sequence is retained
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        m_curSequence = loop;

        // Recursively work out any other pre-processors
        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);

        return;
    }

    // Check for the end of the loop
    if ((block->GetBlockID() == icarusToken_t::ID_BLOCK_END) && (m_curSequence->HasFlag(sequenceFlags_t::SQ_LOOP))) {
        // We don't want to decrement -1
        if (m_curSequence->GetIterations() > 0)
            m_curSequence->SetIterations(m_curSequence->GetIterations() - 1); // Nice, eh?

        // Either there's another iteration, or it's infinite
        if (m_curSequence->GetIterations() != 0) {
            // Another iteration is going to happen, so this will need to be considered again
            PushCommand(block, pushPop_t::PUSH_FRONT);

            *command = PopCommand(pushPop_t::POP_BACK);
            Prep(command);

            return;
        }
        else {
            assert(m_curSequence->GetReturn());
            if (m_curSequence->GetReturn() == nullptr) {
                *command = nullptr;
                return;
            }

            // Check to retain it
            if (m_curSequence->GetParent()->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
                PushCommand(block, pushPop_t::PUSH_FRONT);
            }
            else {
                delete block;
                block = nullptr;
                *command = nullptr;
            }

            // Back out of the loop and resume the previous sequence
            m_curSequence = ReturnSequence(m_curSequence);

            // This can safely happen
            if (m_curSequence == nullptr) {
                *command = nullptr;
                return;
            }

            *command = PopCommand(pushPop_t::POP_BACK);
            Prep(command);
        }
    }
}

/*
========================
CheckFlush

Checks for flush command pre-processing
========================
*/

void CSequencer::CheckFlush(CBlock** command)
{
    sequence_l::iterator sli;
    CBlock* block = *command;

    if (block == nullptr)
        return;

    if (block->GetBlockID() == icarusToken_t::ID_FLUSH) {
        // Flush the sequence
        Flush(m_curSequence);

        // Check to retain it
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);

        return;
    }
}

/*
========================
CheckAffect

Checks for affect command pre-processing
========================
*/

void CSequencer::CheckAffect(CBlock** command)
{
    CBlock* block = *command;
    gentity_t* ent = nullptr;
    char* entname = nullptr;
    int memberNum = 0;

    if (block == nullptr) {
        return;
    }

    if (block->GetBlockID() == icarusToken_t::ID_AFFECT) {
        CSequencer* sequencer = nullptr;
        entname = (char*)block->GetMemberData(memberNum++);
        ent = m_ie->I_GetEntityByName(entname);

        if (!ent) // if there wasn't a valid entname in the affect, we need to check if it's a get command
        {
            // try to parse a 'get' command that is embeded in this 'affect'

            icarusToken_t id;
            const char* p1 = nullptr;
            char* name = nullptr;
            CBlockMember* bm = nullptr;
            //
            //	Get the first parameter (this should be the get)
            //
            bm = block->GetMember(0);
            id = bm->GetID();

            switch (id) {
            // these 3 cases probably aren't necessary
            case icarusToken_t::TK_STRING:
            case icarusToken_t::TK_IDENTIFIER:
            case icarusToken_t::TK_CHAR:
                p1 = (char*)bm->GetData();
                break;

            case icarusToken_t::ID_GET: {
                icarusToken_t type;

                // get( TYPE, NAME )
                type = block->GetMemberToken(memberNum++);
                name = (char*)block->GetMemberData(memberNum++);

                switch (type) // what type are they attempting to get
                {

                case icarusToken_t::TK_STRING:
                    // only string is acceptable for affect, store result in p1
                    if (m_ie->I_GetString(m_ownerID, type, name, &p1) == false) {
                        return;
                    }
                    break;
                default:
                    // FIXME: Make an enum id for the error...
                    m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on affect _1");
                    return;
                    break;
                }

                break;
            }

            default:
                // FIXME: Make an enum id for the error...
                m_ie->I_DPrintf(WL_e::WL_ERROR, "Invalid parameter type on affect _2");
                return;
                break;
            } // end id switch

            if (p1) {
                ent = m_ie->I_GetEntityByName(p1);
            }
            if (!ent) { // a valid entity name was not returned from the get command
                m_ie->I_DPrintf(WL_e::WL_WARNING, "'%s' : invalid affect() target\n", p1 ? p1 : "");
            }

        } // end if(!ent)

        if (ent) {
            sequencer = ent->sequencer;
        }
        if (memberNum == 0) { // there was no get, increment manually before next step
            memberNum++;
        }
        icarusToken_t type = block->GetMemberToken(memberNum);
        int id = (int)(*(float*)block->GetMemberData(memberNum + 1));

        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        // NOTENOTE: If this isn't found, continue on to the next command
        if (sequencer == nullptr) {
            *command = PopCommand(pushPop_t::POP_BACK);
            Prep(command);
            return;
        }

        sequencer->Affect(id, type);

        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);
        if (ent) { // ents need to update upon being affected
            ent->taskManager->Update();
        }

        return;
    }

    if ((block->GetBlockID() == icarusToken_t::ID_BLOCK_END) && (m_curSequence->HasFlag(sequenceFlags_t::SQ_AFFECT))) {
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        m_curSequence = ReturnSequence(m_curSequence);

        if (m_curSequence == nullptr) {
            *command = nullptr;
            return;
        }

        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);
        if (ent) { // ents need to update upon being affected
            ent->taskManager->Update();
        }
    }
}

/*
-------------------------
CheckDo
-------------------------
*/

void CSequencer::CheckDo(CBlock** command)
{
    CBlock* block = *command;

    if (block == nullptr)
        return;

    if (block->GetBlockID() == icarusToken_t::ID_DO) {
        // Get the sequence
        const char* groupName = (const char*)block->GetMemberData(0);
        CTaskGroup* group = m_taskManager->GetTaskGroup(groupName);
        CSequence* sequence = GetTaskSequence(group);

        // TODO: Emit warning
        assert(group);
        if (group == nullptr) {
            // TODO: Give name/number of entity trying to execute, too
            m_ie->I_DPrintf(WL_e::WL_ERROR, "ICARUS Unable to find task group \"%s\"!\n", groupName);
            *command = nullptr;
            return;
        }

        // TODO: Emit warning
        assert(sequence);
        if (sequence == nullptr) {
            // TODO: Give name/number of entity trying to execute, too
            m_ie->I_DPrintf(WL_e::WL_ERROR, "ICARUS Unable to find task 'group' sequence!\n");
            *command = nullptr;
            return;
        }

        // Only save the loop command if the calling sequence is retained
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        // Set this to our current sequence
        sequence->SetReturn(m_curSequence);
        m_curSequence = sequence;

        group->SetParent(m_curGroup);
        m_curGroup = group;

        // Mark all the following commands as being in the task
        m_taskManager->MarkTask(group->GetGUID(), taskMark_t::TASK_START);

        // Recursively work out any other pre-processors
        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);

        return;
    }

    if ((block->GetBlockID() == icarusToken_t::ID_BLOCK_END) && (m_curSequence->HasFlag(sequenceFlags_t::SQ_TASK))) {
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
            *command = nullptr;
        }

        m_taskManager->MarkTask(m_curGroup->GetGUID(), taskMark_t::TASK_END);
        m_curGroup = m_curGroup->GetParent();

        CSequence* returnSeq = ReturnSequence(m_curSequence);
        m_curSequence->SetReturn(nullptr);
        m_curSequence = returnSeq;

        if (m_curSequence == nullptr) {
            *command = nullptr;
            return;
        }

        *command = PopCommand(pushPop_t::POP_BACK);
        Prep(command);
    }
}

/*
========================
Prep

Handles internal sequencer maintenance
========================
*/

void CSequencer::Prep(CBlock** command)
{
    // Check all pre-processes
    CheckAffect(command);
    CheckFlush(command);
    CheckLoop(command);
    CheckRun(command);
    CheckIf(command);
    CheckDo(command);
}

/*
========================
Prime

Starts communication between the task manager and this sequencer
========================
*/

sequencerResult_t CSequencer::Prime(CTaskManager* taskManager, CBlock* command)
{
    Prep(&command);

    if (command) {
        taskManager->SetCommand(command, pushPop_t::PUSH_BACK);
    }

    return sequencerResult_t::SEQ_OK;
}

/*
========================
Callback

Handles a completed task and returns a new task to be completed
========================
*/

sequencerResult_t CSequencer::Callback(CTaskManager* taskManager, CBlock* block, taskReturn_t returnCode)
{
    CBlock* command;

    if (returnCode == taskReturn_t::TASK_RETURN_COMPLETE) {
        // There are no more pending commands
        if (m_curSequence == nullptr) {
            delete block;
            block = nullptr;
            return sequencerResult_t::SEQ_OK;
        }

        // Check to retain the command
        if (m_curSequence->HasFlag(sequenceFlags_t::SQ_RETAIN)) // This isn't true for affect sequences...?
        {
            PushCommand(block, pushPop_t::PUSH_FRONT);
        }
        else {
            delete block;
            block = nullptr;
        }

        // Check for pending commands
        if (m_curSequence->GetNumCommands() <= 0) {
            if (m_curSequence->GetReturn() == nullptr)
                return sequencerResult_t::SEQ_OK;

            m_curSequence = m_curSequence->GetReturn();
        }

        command = PopCommand(pushPop_t::POP_BACK);
        Prep(&command);

        if (command)
            taskManager->SetCommand(command, pushPop_t::PUSH_FRONT);

        return sequencerResult_t::SEQ_OK;
    }

    // FIXME: This could be more descriptive
    m_ie->I_DPrintf(WL_e::WL_ERROR, "command could not be called back\n");
    assert(0);

    return sequencerResult_t::SEQ_FAILED;
}

/*
-------------------------
Recall
-------------------------
*/

int CSequencer::Recall(void)
{
    CBlock* block = nullptr;

    while ((block = m_taskManager->RecallTask()) != nullptr) {
        if (m_curSequence) {
            PushCommand(block, pushPop_t::PUSH_BACK);
        }
        else {
            delete block;
            block = nullptr;
        }
    }

    return true;
}

/*
-------------------------
Affect
-------------------------
*/

sequencerResult_t CSequencer::Affect(int id, icarusToken_t type)
{
    CSequence* sequence = GetSequence(id);

    if (sequence == nullptr) {
        return sequencerResult_t::SEQ_FAILED;
    }

    switch (type) {

    case icarusToken_t::TYPE_FLUSH:

        // Get rid of all old code
        Flush(sequence);

        sequence->RemoveFlag(sequenceFlags_t::SQ_PENDING, true);

        m_curSequence = sequence;

        Prime(m_taskManager, PopCommand(pushPop_t::POP_BACK));

        break;

    case icarusToken_t::TYPE_INSERT:

        Recall();

        sequence->SetReturn(m_curSequence);

        sequence->RemoveFlag(sequenceFlags_t::SQ_PENDING, true);

        m_curSequence = sequence;

        Prime(m_taskManager, PopCommand(pushPop_t::POP_BACK));

        break;

    default:
        m_ie->I_DPrintf(WL_e::WL_ERROR, "unknown affect type found");
        break;
    }

    return sequencerResult_t::SEQ_OK;
}

/*
========================
PushCommand

Pushes a commands onto the current sequence
========================
*/

sequencerResult_t CSequencer::PushCommand(CBlock* command, pushPop_t flag)
{
    // Make sure everything is ok
    assert(m_curSequence);
    if (m_curSequence == nullptr)
        return sequencerResult_t::SEQ_FAILED;

    m_curSequence->PushCommand(command, flag);
    m_numCommands++;

    // Invalid flag
    return sequencerResult_t::SEQ_OK;
}

/*
========================
PopCommand

Pops a command off the current sequence
========================
*/

CBlock* CSequencer::PopCommand(pushPop_t flag)
{
    // Make sure everything is ok
    assert(m_curSequence);
    if (m_curSequence == nullptr)
        return nullptr;

    CBlock* block = m_curSequence->PopCommand(flag);

    if (block != nullptr)
        m_numCommands--;

    return block;
}

/*
-------------------------
RemoveSequence
-------------------------
*/

// NOTENOTE: This only removes references to the sequence, IT DOES NOT FREE THE ALLOCATED MEMORY!  You've be warned! =)

sequencerResult_t CSequencer::RemoveSequence(CSequence* sequence)
{
    CSequence* temp;

    int numChildren = sequence->GetNumChildren();

    // Add all the children
    for (int i = 0; i < numChildren; i++) {
        temp = sequence->GetChild(i);

        // TODO: Emit warning
        assert(temp);
        if (temp == nullptr) {
            m_ie->I_DPrintf(WL_e::WL_WARNING, "Unable to find child sequence on RemoveSequence call!\n");
            continue;
        }

        // Remove the references to this sequence
        temp->SetParent(nullptr);
        temp->SetReturn(nullptr);
    }

    return sequencerResult_t::SEQ_OK;
}

sequencerResult_t CSequencer::DestroySequence(CSequence* sequence)
{
    m_sequenceMap.erase(sequence->GetID());
    m_sequences.remove(sequence);

    taskSequence_m::iterator tsi;
    for (tsi = m_taskSequences.begin(); tsi != m_taskSequences.end();) {
        if ((*tsi).second == sequence) {
            m_taskSequences.erase(tsi++);
        }
        else {
            ++tsi;
        }
    }

    CSequence* parent = sequence->GetParent();
    if (parent) {
        parent->RemoveChild(sequence);
        parent = nullptr;
    }

    int curChild = sequence->GetNumChildren();
    while (curChild) {
        curChild--;
        DestroySequence(sequence->GetChild(curChild));
    }

    m_owner->DeleteSequence(sequence);

    return sequencerResult_t::SEQ_OK;
}

/*
-------------------------
ReturnSequence
-------------------------
*/

inline CSequence* CSequencer::ReturnSequence(CSequence* sequence)
{
    while (sequence->GetReturn()) {
        assert(sequence != sequence->GetReturn());
        if (sequence == sequence->GetReturn())
            return nullptr;

        sequence = sequence->GetReturn();

        if (sequence->GetNumCommands() > 0)
            return sequence;
    }
    return nullptr;
}

// Save / Load

/*
-------------------------
Save
-------------------------
*/

int CSequencer::Save(void)
{
    sequence_l::iterator si;
    taskSequence_m::iterator ti;
    int numSequences = 0, id, numTasks;

    // Get the number of sequences to save out
    numSequences = m_sequences.size();

    ojk::SavedGameHelper saved_game(m_ie->saved_game);

    // Save out the owner sequence
    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'R', 'E'), m_ownerID);

    // Write out the number of sequences we need to read
    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'R', '#'), numSequences);

    // Second pass, save out all sequences, in order
    STL_ITERATE(si, m_sequences)
    {
        id = (*si)->GetID();

        saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'R', 'I'), id);
    }

    // Save out the taskManager
    m_taskManager->Save();

    // Save out the task sequences mapping the name to the GUIDs
    numTasks = m_taskSequences.size();

    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'T', '#'), numTasks);

    STL_ITERATE(ti, m_taskSequences)
    {
        // Save the task group's ID
        id = ((*ti).first)->GetGUID();

        saved_game.write_chunk<int32_t>(INT_ID('S', 'T', 'I', 'D'), id);

        // Save the sequence's ID
        id = ((*ti).second)->GetID();

        saved_game.write_chunk<int32_t>(INT_ID('S', 'S', 'I', 'D'), id);
    }

    int curGroupID = (m_curGroup == nullptr) ? -1 : m_curGroup->GetGUID();

    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'C', 'T'), curGroupID);

    // Output the number of commands
    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', '#', 'C'),
                                    m_numCommands); // FIXME: This can be reconstructed

    // Output the ID of the current sequence
    id = (m_curSequence != nullptr) ? m_curSequence->GetID() : -1;

    saved_game.write_chunk<int32_t>(INT_ID('S', 'Q', 'C', 'S'), id);

    return true;
}

/*
-------------------------
Load
-------------------------
*/

int CSequencer::Load(void)
{
    int i;

    ojk::SavedGameHelper saved_game(m_ie->saved_game);

    // Get the owner of this sequencer
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'R', 'E'), m_ownerID);

    // Link the entity back to the sequencer
    m_ie->I_LinkEntity(m_ownerID, this, m_taskManager);

    CTaskGroup* taskGroup;
    CSequence* seq;
    int numSequences = 0, seqID = 0, taskID = 0, numTasks = 0;

    // Get the number of sequences to read
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'R', '#'), numSequences);

    // Read in all the sequences
    for (i = 0; i < numSequences; i++) {
        saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'R', 'I'), seqID);

        seq = m_owner->GetSequence(seqID);

        assert(seq);

        STL_INSERT(m_sequences, seq);
        m_sequenceMap[seqID] = seq;
    }

    // Setup the task manager
    m_taskManager->Init(this);

    // Load the task manager
    m_taskManager->Load();

    // Get the number of tasks in the map
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'T', '#'), numTasks);

    // Read in, and reassociate the tasks to the sequences
    for (i = 0; i < numTasks; i++) {
        // Read in the task's ID
        saved_game.read_chunk<int32_t>(INT_ID('S', 'T', 'I', 'D'), taskID);

        // Read in the sequence's ID
        saved_game.read_chunk<int32_t>(INT_ID('S', 'S', 'I', 'D'), seqID);

        taskGroup = m_taskManager->GetTaskGroup(taskID);

        assert(taskGroup);

        seq = m_owner->GetSequence(seqID);

        assert(seq);

        // Associate the values
        m_taskSequences[taskGroup] = seq;
    }

    int curGroupID = 0;

    // Get the current task group
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'C', 'T'), curGroupID);

    m_curGroup = (curGroupID == -1) ? nullptr : m_taskManager->GetTaskGroup(curGroupID);

    // Get the number of commands
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', '#', 'C'), m_numCommands);

    // Get the current sequence
    saved_game.read_chunk<int32_t>(INT_ID('S', 'Q', 'C', 'S'), seqID);

    m_curSequence = (seqID != -1) ? m_owner->GetSequence(seqID) : nullptr;

    return true;
}
