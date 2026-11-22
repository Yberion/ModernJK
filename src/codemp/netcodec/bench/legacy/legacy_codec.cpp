/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// Engine-side glue of the legacy benchmark (see legacy_codec.h). Compiled like the engine (C++20, ModernJK headers).
// The loops below follow codemp/server/sv_snapshot.cpp and codemp/client/cl_parse.cpp; the engine's msg functions
// take non-const pointers but do not modify their sources, hence the const_casts.
#include "legacy_codec.h"

#include <qcommon/q_shared.h>
#include <qcommon/qcommon.h>

#include <cstring>

static_assert(sizeof(entityState_t) == legacy::kEntityBytes, "entityState_t layout");
static_assert(sizeof(playerState_t) == legacy::kPlayerBytes, "playerState_t layout");

namespace legacy {

namespace {

entityState_t* Ent(const uint8_t* img)
{
    return reinterpret_cast<entityState_t*>(const_cast<uint8_t*>(img));
}
playerState_t* Ps(const uint8_t* img)
{
    return reinterpret_cast<playerState_t*>(const_cast<uint8_t*>(img));
}

const playerState_t& ZeroPs()
{
    static playerState_t zero;
    return zero;
}

// SV_EmitPacketEntities
void EmitPacketEntities(msg_t* msg, const SnapshotView* from, const SnapshotView& to, const uint8_t* const* baselines)
{
    const int fromNum = from ? from->numEntities : 0;
    int newindex = 0, oldindex = 0;
    while (newindex < to.numEntities || oldindex < fromNum) {
        entityState_t* newent = newindex < to.numEntities ? Ent(to.entities[newindex]) : nullptr;
        entityState_t* oldent = oldindex < fromNum ? Ent(from->entities[oldindex]) : nullptr;
        const int newnum = newent ? newent->number : 9999;
        const int oldnum = oldent ? oldent->number : 9999;
        if (newnum == oldnum) {
            MSG_WriteDeltaEntity(msg, oldent, newent, false);
            oldindex++;
            newindex++;
        }
        else if (newnum < oldnum) {
            MSG_WriteDeltaEntity(msg, Ent(baselines[newnum]), newent, true);
            newindex++;
        }
        else {
            MSG_WriteDeltaEntity(msg, oldent, nullptr, true);
            oldindex++;
        }
    }
    MSG_WriteBits(msg, MAX_GENTITIES - 1, GENTITYNUM_BITS);
}

} // namespace

int Encode(const MessageView& m, const SnapshotView* base, const uint8_t* const* baselines, uint8_t* out, int capacity)
{
    msg_t msg;
    MSG_Init(&msg, out, capacity);
    MSG_WriteLong(&msg, m.ack);               // SV_SendClientSnapshot: lastClientCommand
    for (int i = 0; i < m.numCommands; i++) { // SV_UpdateServerCommandsToClient
        MSG_WriteByte(&msg, std::to_underlying(svc_ops_e::svc_serverCommand));
        MSG_WriteLong(&msg, m.commands[i].seq);
        MSG_WriteString(&msg, m.commands[i].text);
    }
    if (m.hasSnapshot) { // SV_WriteSnapshotToClient
        MSG_WriteByte(&msg, std::to_underlying(svc_ops_e::svc_snapshot));
        MSG_WriteLong(&msg, m.serverTime);
        MSG_WriteByte(&msg, m.delta);
        MSG_WriteByte(&msg, m.flags);
        MSG_WriteByte(&msg, m.numAreas);
        MSG_WriteData(&msg, m.areas, m.numAreas);
        playerState_t* ps = Ps(m.snap.ps);
        playerState_t* vps = m.snap.vps ? Ps(m.snap.vps) : const_cast<playerState_t*>(&ZeroPs());
        if (base) {
            MSG_WriteDeltaPlayerstate(&msg, Ps(base->ps), ps);
            if (ps->m_iVehicleNum) {
                if (!Ps(base->ps)->m_iVehicleNum)
                    MSG_WriteDeltaPlayerstate(&msg, nullptr, vps, true);
                else
                    MSG_WriteDeltaPlayerstate(&msg, base->vps ? Ps(base->vps) : const_cast<playerState_t*>(&ZeroPs()), vps, true);
            }
        }
        else {
            MSG_WriteDeltaPlayerstate(&msg, nullptr, ps);
            if (ps->m_iVehicleNum)
                MSG_WriteDeltaPlayerstate(&msg, nullptr, vps, true);
        }
        EmitPacketEntities(&msg, base, m.snap, baselines);
    }
    MSG_WriteByte(&msg, std::to_underlying(svc_ops_e::svc_EOF));
    return msg.overflowed ? 0 : msg.cursize;
}

bool Decode(const uint8_t* data, int size, const SnapshotView* base, const uint8_t* const* baselines, SnapshotOut& out)
{
    msg_t msg;
    MSG_Init(&msg, const_cast<uint8_t*>(data), size);
    msg.cursize = size;
    MSG_BeginReading(&msg);
    MSG_ReadLong(&msg); // CL_ParseServerMessage: reliableAcknowledge
    out.numEntities = 0;
    for (;;) {
        const auto cmd = static_cast<svc_ops_e>(MSG_ReadByte(&msg)); // a byte of the message
        if (cmd == svc_ops_e::svc_EOF || msg.readcount > msg.cursize)
            break;
        if (cmd == svc_ops_e::svc_serverCommand) {
            MSG_ReadLong(&msg);
            MSG_ReadString(&msg);
        }
        else if (cmd == svc_ops_e::svc_snapshot) { // CL_ParseSnapshot
            MSG_ReadLong(&msg);
            MSG_ReadByte(&msg);
            MSG_ReadByte(&msg);
            byte areamask[MAX_MAP_AREA_BYTES];
            const int len = MSG_ReadByte(&msg);
            if (len > static_cast<int>(sizeof(areamask)))
                return false;
            MSG_ReadData(&msg, areamask, len);
            playerState_t* ps = reinterpret_cast<playerState_t*>(out.ps);
            playerState_t* vps = reinterpret_cast<playerState_t*>(out.vps);
            if (base) {
                MSG_ReadDeltaPlayerstate(&msg, Ps(base->ps), ps);
                if (ps->m_iVehicleNum)
                    MSG_ReadDeltaPlayerstate(&msg, base->vps ? Ps(base->vps) : const_cast<playerState_t*>(&ZeroPs()), vps, true);
            }
            else {
                MSG_ReadDeltaPlayerstate(&msg, nullptr, ps);
                if (ps->m_iVehicleNum)
                    MSG_ReadDeltaPlayerstate(&msg, nullptr, vps, true);
            }
            // CL_ParsePacketEntities and CL_DeltaEntity
            entityState_t* ents = reinterpret_cast<entityState_t*>(out.entities);
            const int oldCount = base ? base->numEntities : 0;
            int oldindex = 0;
            int oldnum = oldindex < oldCount ? Ent(base->entities[oldindex])->number : 99999;
            const auto keep = [&](entityState_t* state) {
                if (state->number != MAX_GENTITIES - 1)
                    out.numEntities++;
            };
            for (;;) {
                const int newnum = MSG_ReadBits(&msg, GENTITYNUM_BITS);
                if (newnum == MAX_GENTITIES - 1)
                    break;
                if (msg.readcount > msg.cursize || out.numEntities >= kMaxEntities)
                    return false;
                while (oldnum < newnum) {
                    ents[out.numEntities] = *Ent(base->entities[oldindex]);
                    keep(&ents[out.numEntities]);
                    oldindex++;
                    oldnum = oldindex < oldCount ? Ent(base->entities[oldindex])->number : 99999;
                }
                if (oldnum == newnum) {
                    MSG_ReadDeltaEntity(&msg, Ent(base->entities[oldindex]), &ents[out.numEntities], newnum);
                    keep(&ents[out.numEntities]);
                    oldindex++;
                    oldnum = oldindex < oldCount ? Ent(base->entities[oldindex])->number : 99999;
                }
                else {
                    MSG_ReadDeltaEntity(&msg, Ent(baselines[newnum]), &ents[out.numEntities], newnum);
                    keep(&ents[out.numEntities]);
                }
            }
            while (oldnum != 99999) {
                ents[out.numEntities] = *Ent(base->entities[oldindex]);
                keep(&ents[out.numEntities]);
                oldindex++;
                oldnum = oldindex < oldCount ? Ent(base->entities[oldindex])->number : 99999;
            }
        }
        else {
            return false; // the benchmark sends only commands and snapshots
        }
    }
    return msg.readcount <= msg.cursize;
}

static_assert(sizeof(usercmd_t) == kUserCmdBytes, "usercmd_t layout");
static_assert(MAX_PACKET_USERCMDS == kMaxUserCmds, "user commands per packet");

int EncodeClientPacket(const ClientPacketView& p, uint8_t* out, int capacity)
{
    msg_t msg;
    MSG_Init(&msg, out, capacity);
    MSG_Bitstream(&msg);
    MSG_WriteLong(&msg, p.serverId);
    MSG_WriteLong(&msg, p.messageAck);
    MSG_WriteLong(&msg, p.reliableAck);
    for (int i = 0; i < p.numCommands; i++) {
        MSG_WriteByte(&msg, std::to_underlying(clc_ops_e::clc_clientCommand));
        MSG_WriteLong(&msg, p.commands[i].seq);
        MSG_WriteString(&msg, p.commands[i].text);
    }
    if (p.move != Move::kNone) {
        MSG_WriteByte(&msg, std::to_underlying(p.move == Move::kDelta ? clc_ops_e::clc_move : clc_ops_e::clc_moveNoDelta));
        MSG_WriteByte(&msg, p.numUserCmds);
        usercmd_t nullcmd;
        std::memset(&nullcmd, 0, sizeof(nullcmd));
        usercmd_t* oldcmd = &nullcmd;
        for (int i = 0; i < p.numUserCmds; i++) {
            usercmd_t* cmd = reinterpret_cast<usercmd_t*>(const_cast<uint8_t*>(p.userCmds + i * kUserCmdBytes));
            MSG_WriteDeltaUsercmdKey(&msg, p.key, oldcmd, cmd);
            oldcmd = cmd;
        }
    }
    MSG_WriteByte(&msg, std::to_underlying(clc_ops_e::clc_EOF));
    return msg.overflowed ? 0 : msg.cursize;
}

bool DecodeClientPacket(const uint8_t* data, int size, int32_t key, ClientPacketOut& out)
{
    msg_t msg;
    MSG_Init(&msg, const_cast<uint8_t*>(data), size);
    msg.cursize = size;
    MSG_BeginReading(&msg);
    out.serverId = MSG_ReadLong(&msg);
    out.messageAck = MSG_ReadLong(&msg);
    out.reliableAck = MSG_ReadLong(&msg);
    out.headerBits = msg.bit;
    out.numCommands = 0;
    out.move = Move::kNone;
    out.numUserCmds = 0;
    if (out.messageAck < 0)
        return false;
    int textUsed = 0;
    clc_ops_e c{};
    for (;;) {
        c = static_cast<clc_ops_e>(MSG_ReadByte(&msg)); // a byte of the packet
        if (c != clc_ops_e::clc_clientCommand || msg.readcount > msg.cursize)
            break;
        if (out.numCommands == ClientPacketOut::kMaxCommands)
            return false;
        Command& command = out.commands[out.numCommands++];
        command.seq = MSG_ReadLong(&msg);
        const char* s = MSG_ReadString(&msg);
        const int len = static_cast<int>(std::strlen(s)) + 1;
        if (textUsed + len > static_cast<int>(sizeof(out.text)))
            return false;
        std::memcpy(out.text + textUsed, s, len);
        command.text = out.text + textUsed;
        textUsed += len;
    }
    out.commandBits = msg.bit - out.headerBits;
    if (c == clc_ops_e::clc_move || c == clc_ops_e::clc_moveNoDelta) {
        out.move = c == clc_ops_e::clc_move ? Move::kDelta : Move::kNoDelta;
        out.numUserCmds = MSG_ReadByte(&msg);
        if (out.numUserCmds < 1 || out.numUserCmds > kMaxUserCmds)
            return false;
        usercmd_t nullcmd;
        std::memset(&nullcmd, 0, sizeof(nullcmd));
        usercmd_t* oldcmd = &nullcmd;
        for (int i = 0; i < out.numUserCmds; i++) {
            usercmd_t* cmd = reinterpret_cast<usercmd_t*>(out.userCmds + i * kUserCmdBytes);
            std::memset(cmd, 0, sizeof(*cmd));
            MSG_ReadDeltaUsercmdKey(&msg, key, oldcmd, cmd);
            oldcmd = cmd;
        }
        MSG_ReadByte(&msg); // clc_EOF (the server does not read it)
    }
    else if (c != clc_ops_e::clc_EOF) {
        return false;
    }
    out.moveBits = msg.bit - out.headerBits - out.commandBits;
    return msg.readcount <= msg.cursize;
}

} // namespace legacy
