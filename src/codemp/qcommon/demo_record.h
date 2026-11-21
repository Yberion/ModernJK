/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// demo_record.h -- the demos of the client (record) and of the server (svrecord, sv_autoDemo), coded for themselves by
// the message codec (plan step 7) with one snapshot in two (plan step 14, part 11).
//
// A demo is a stream of its own: each message refers only to messages of the demo, and the first one is a gamestate
// holding the server commands later ones may be coded against. Of the snapshots, the demo keeps one every
// DEMO_SNAPSHOT_MSEC and leaves out the others, unless a snapshot left out holds an event the next one no longer shows
// (an event stays in its field until a new one replaces it): that snapshot is kept too, so that no event is lost. The
// player draws the demo between the snapshots it holds. A message left out gives its server commands to the next one
// written; a download block is never recorded.

#pragma once

#include "qcommon/qcommon.h"
#include "netcodec/engine.h"

// What a demo's messages are coded against, owned by the engine: the entity ring and the next index the engine will
// write in it, the server command ring (its last command is given with each message) and the baselines.
struct demoSources_t {
    netcodec::engine::EntityRing ring;
    int64_t nextEntity;
    netcodec::engine::CommandRing commands;
    netcodec::engine::Baselines baselines;
};

class DemoRecorder {
public:
    // The demo's first message, a gamestate (config strings, the baselines of src, the server commands from its
    // firstCommand to lastCommand); false if it could not be coded.
    bool Start(fileHandle_t file, const netcodec::engine::ServerMessage& gameState, int lastCommand, const demoSources_t& src);

    // A message of the stream, whose server commands end at lastCommand: written, held back until the next one shows
    // whether it must be, or left out.
    void Message(const netcodec::engine::ServerMessage& msg, int lastCommand, const demoSources_t& src);

    // The end of the demo: the message held back is written.
    void Stop(const demoSources_t& src);

    // The engine cleared the snapshots the demo's frames point to (the client, before a gamestate): nothing is held
    // back and no earlier snapshot is a base; the server commands not written yet go with the next message.
    void Reset(void);

private:
    bool Write(const netcodec::engine::ServerMessage& msg, int lastCommand, const demoSources_t& src);
    void ResetFrames(void);

    fileHandle_t file;
    netcodec::engine::Frame frames[netcodec::engine::kPacketBackup]; // the demo's snapshots, slot seq % kPacketBackup
    int keptSeq;                                                     // the demo's last snapshot, the base of the next one; -1: none
    int keptTime;
    int nextCommand; // the first server command the demo does not hold yet
    bool held;
    netcodec::engine::ServerMessage heldMsg; // a snapshot message held back
    int heldLastCommand;
    uint8_t heldAreas[MAX_MAP_AREA_BYTES];
};
