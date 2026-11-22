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

// NPC_sounds.cpp

#include "g_headers.h"

#include "b_local.h"
#include "Q3_Interface.h"

/*
void NPC_AngerSound (void)
{
    if(NPCInfo->investigateSoundDebounceTime)
        return;

    NPCInfo->investigateSoundDebounceTime = 1;

//	switch((int)NPC->client->race)
//	{
//	case RACE_KLINGON:
        //G_Sound(NPC, G_SoundIndex(Q_Format("sound/mgtest/klingon/talk{}.wav", Q_irand(1, 4)).c_str()));
//		break;
//	}
}
*/

extern void G_SpeechEvent(gentity_t* self, entity_event_t event);
void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime)
{
    if (!self->NPC) {
        return;
    }

    if (!self->client || self->client->ps.pm_type >= pmtype_t::PM_DEAD) {
        return;
    }

    if (self->NPC->blockedSpeechDebounceTime > level.time) {
        return;
    }

    if (Q3_TaskIDPending(self, taskID_t::TID_CHAN_VOICE)) {
        return;
    }

    if (Any(self->NPC->scriptFlags & scriptFlags_t::SCF_NO_COMBAT_TALK) &&
        ((event >= entity_event_t::EV_ANGER1 && event <= entity_event_t::EV_VICTORY3) ||
         (event >= entity_event_t::EV_CHASE1 &&
          event <= entity_event_t::EV_SUSPICIOUS5))) //(event < EV_FF_1A || event > EV_FF_3C) && (event < EV_RESPOND1 || event > EV_MISSION3) )
    {
        return;
    }

    if (Any(self->NPC->scriptFlags & scriptFlags_t::SCF_NO_ALERT_TALK) && (event >= entity_event_t::EV_GIVEUP1 && event <= entity_event_t::EV_SUSPICIOUS5)) {
        return;
    }
    // FIXME: Also needs to check for teammates. Don't want
    //		everyone babbling at once

    // NOTE: was losing too many speech events, so we do it directly now, screw networking!
    // G_AddEvent( self, event, 0 );
    G_SpeechEvent(self, event);

    // won't speak again for 5 seconds (unless otherwise specified)
    self->NPC->blockedSpeechDebounceTime = level.time + ((speakDebounceTime == 0) ? 5000 : speakDebounceTime);
}

void NPC_PlayConfusionSound(gentity_t* self)
{
    if (self->health > 0) {
        if (self->enemy ||                           // was mad
            !TIMER_Done(self, "enemyLastVisible") || // saw something suspicious
            self->client->renderInfo.lookTarget == 0 // was looking at player
        ) {
            self->NPC->blockedSpeechDebounceTime = 0; // make sure we say this
            G_AddVoiceEvent(self, Q_irand(entity_event_t::EV_CONFUSE2, entity_event_t::EV_CONFUSE3), 2000);
        }
        else if (self->NPC && self->NPC->investigateDebounceTime + self->NPC->pauseTime > level.time) // was checking something out
        {
            self->NPC->blockedSpeechDebounceTime = 0; // make sure we say this
            G_AddVoiceEvent(self, entity_event_t::EV_CONFUSE1, 2000);
        }
        // G_AddVoiceEvent( self, Q_irand(entity_event_t::EV_CONFUSE1, entity_event_t::EV_CONFUSE3), 2000 );
    }
    // reset him to be totally unaware again
    TIMER_Set(self, "enemyLastVisible", 0);
    self->NPC->tempBehavior = bState_t::BS_DEFAULT;

    // self->NPC->behaviorState = bState_t::BS_PATROL;
    G_ClearEnemy(self); // FIXME: or just self->enemy = nullptr;?

    self->NPC->investigateCount = 0;
}
