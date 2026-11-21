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

// demo_record.cpp -- the demos of the client and of the server, coded for themselves (see demo_record.h)

#include "demo_record.h"

namespace eng = netcodec::engine;

// one snapshot every two ticks: 50 per second, the half of the stream's
constexpr int DEMO_SNAPSHOT_MSEC = 2 * TICK_MSEC;

// one encoder for every demo: it keeps only scratch buffers
static eng::ServerCodec& DemoCodec(void)
{
    static eng::ServerCodec codec;
    return codec;
}

// entity i of a frame
static const entityState_t* FrameEntity(const eng::Frame& f, const eng::EntityRing& ring, uint32_t i)
{
    const uint32_t n = f.index ? f.index[i] % ring.count : (uint32_t)((f.firstEntity + i) % ring.count);
    return (const entityState_t*)ring.states + n;
}

// the event field of entity `number` in frame f, -1 if the frame does not hold it; a frame lists its entities by
// increasing number, and *at is where the search goes on from (the numbers asked increase too)
static int FrameEvent(const eng::Frame& f, const eng::EntityRing& ring, uint32_t* at, int number)
{
    for (; *at < f.numEntities; (*at)++) {
        const entityState_t* es = FrameEntity(f, ring, *at);
        if (es->number == number) {
            return es->event;
        }
        if (es->number > number) {
            break;
        }
    }
    return -1;
}

// Whether snapshot s, left out between the demo's last snapshot k and the next one n, holds an event the demo would
// lose: an entity's event field, or the player's external event, that s shows new and n no longer shows; or more of
// the player's predictable events from k to n than its player state holds (MAX_PS_EVENTS).
static bool EventsLost(const eng::Frame& k, const eng::Frame& s, const eng::Frame& n, const eng::EntityRing& ring)
{
    const playerState_t* kps = (const playerState_t*)k.ps;
    const playerState_t* sps = (const playerState_t*)s.ps;
    const playerState_t* nps = (const playerState_t*)n.ps;
    uint32_t atK = 0, atN = 0;

    if (nps->eventSequence - kps->eventSequence > MAX_PS_EVENTS) {
        return true;
    }
    if (sps->externalEvent && sps->externalEvent != kps->externalEvent && nps->externalEvent != sps->externalEvent) {
        return true;
    }
    for (uint32_t i = 0; i < s.numEntities; i++) {
        const entityState_t* es = FrameEntity(s, ring, i);
        if (!es->event || FrameEvent(k, ring, &atK, es->number) == es->event) {
            continue;
        }
        if (FrameEvent(n, ring, &atN, es->number) != es->event) {
            return true;
        }
    }
    return false;
}

void DemoRecorder::ResetFrames(void)
{
    for (uint32_t i = 0; i < eng::kPacketBackup; i++) {
        frames[i].seq = -1;
    }
    keptSeq = -1;
}

// codes msg against the demo's last snapshot, with the server commands not written yet, and writes it
bool DemoRecorder::Write(const eng::ServerMessage& msg, int lastCommand, const demoSources_t& src)
{
    static uint8_t buf[MAX_MSGLEN];
    eng::ServerMessage m = msg;
    eng::CommandRing commands = src.commands;
    eng::Record record = {};
    uint32_t delta = 0;
    const int oldest = lastCommand - (int)commands.count + 1; // the ring holds no older command

    commands.last = lastCommand;
    m.firstCommand = nextCommand > oldest ? nextCommand : oldest;
    m.download = false;
    if (m.gameState) {
        ResetFrames(); // a gamestate clears the player's state: no earlier snapshot is a base
    }
    if (m.snapshot) {
        const int distance = m.seq - keptSeq;
        m.delta = keptSeq >= 0 && distance > 0 && distance < (int)eng::kPacketBackup ? (uint32_t)distance : 0;
    }

    const size_t size = DemoCodec().Encode(m, frames, src.ring, src.nextEntity, commands, src.baselines, buf, sizeof(buf), delta, record);

    eng::Frame& slot = frames[(uint32_t)m.seq % eng::kPacketBackup];
    slot.seq = -1;
    if (!size) {
        Com_Printf("WARNING: message %d not recorded in the demo (too large or malformed)\n", m.seq);
        return false;
    }
    if (m.snapshot) {
        slot = m.frame;
        slot.seq = m.seq;
        slot.record = record;
        keptSeq = m.seq;
        keptTime = m.time;
    }
    nextCommand = lastCommand + 1;

    int len = LittleLong(m.seq);
    FS_Write(&len, 4, file);
    len = LittleLong((int)size);
    FS_Write(&len, 4, file);
    FS_Write(buf, (int)size, file);
    return true;
}

bool DemoRecorder::Start(fileHandle_t demoFile, const eng::ServerMessage& gameState, int lastCommand, const demoSources_t& src)
{
    file = demoFile;
    held = false;
    ResetFrames();
    nextCommand = gameState.firstCommand;
    return Write(gameState, lastCommand, src);
}

void DemoRecorder::Message(const eng::ServerMessage& msg, int lastCommand, const demoSources_t& src)
{
    // only a message that holds a snapshot and nothing a player must not miss may be left out
    const bool plain = msg.snapshot && !msg.gameState && !msg.setGame && !msg.mapChange ? true : false;

    if (held) {
        held = false;
        if (!plain || keptSeq < 0 || EventsLost(frames[(uint32_t)keptSeq % eng::kPacketBackup], heldMsg.frame, msg.frame, src.ring)) {
            Write(heldMsg, heldLastCommand, src);
        }
    }

    if (plain && keptSeq >= 0 && msg.time - keptTime < DEMO_SNAPSHOT_MSEC) {
        held = true;
        heldMsg = msg;
        heldMsg.download = false;
        heldMsg.data = nullptr;
        heldLastCommand = lastCommand;
        heldMsg.numAreas = msg.numAreas < sizeof(heldAreas) ? msg.numAreas : (uint32_t)sizeof(heldAreas);
        if (heldMsg.numAreas) {
            Com_Memcpy(heldAreas, msg.areas, heldMsg.numAreas); // the caller's copy may not outlive this call
        }
        heldMsg.areas = heldAreas;
        return;
    }

    Write(msg, lastCommand, src);
}

void DemoRecorder::Stop(const demoSources_t& src)
{
    if (held) {
        held = false;
        Write(heldMsg, heldLastCommand, src);
    }
}

void DemoRecorder::Reset(void)
{
    held = false;
    ResetFrames();
}
