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

// engine.h -- the sound as an engine runs it, compiled by each engine against its own headers (engine.cpp): its
// settings (s_volume, s_volumeVoice, s_musicvolume, s_language, s_muteWhenUnfocused, s_allowDynamicMusic, s_initsound,
// and the cheats s_show and s_debugdynamic), its console commands (play, music, stopmusic, soundlist, soundinfo,
// soundstop, s_dynamic), the game's files read through the engine's file system, its messages in the engine's console,
// and the one sound::Api every module receives, silent when the sound is off.
// Example: the client starts it with SND_Init, calls SND_Frame(cls.realtime) once per frame, gives its modules
// &SND_Api(), and SND_SetFocus(false) when its window loses the focus.
#pragma once

#include <shared/sound/sound.h>

#include <cstdint>
#include <span>

// Starts the sound on the system's default device: its settings and commands; no sound with s_initsound 0 or no device.
void SND_Init();
void SND_Shutdown();
// The sound started again (the snd_restart command): the sounds registered get their handles again, the ambient sets
// and the music come back.
void SND_Restart();
// At every frame, now the client's time in milliseconds: the settings applied, the sound's frame run.
void SND_Frame(int now);
// A level's registration starts, then ends: the sounds the level did not use are freed at its end.
void SND_BeginRegistration();
void SND_EndRegistration();
// Every sound stopped; SND_StopAll stops the music and the cinematics' stream too.
void SND_StopSounds();
void SND_StopAll();
// The window's focus, from the platform's events.
void SND_SetFocus(bool focused);
// The music asked for last, played again from its start.
void SND_RestartMusic();
// The reverb zones of the map a level plays (eagle/<map>.efxzones), from the map's name ("mp/ffa3").
void SND_LoadZones(const char* map);
// The single player's dynamic music: the level it plays; the state its configstring gives, at every frame (a music
// started again goes back to explore until the next frame gives the state).
void SND_SetMusicLevel(const char* level);
void SND_SetMusicState(sound::MusicState state);
// The cinematics' sound: a stream at its format, its samples as they come, closed at the cinematic's end.
void SND_OpenStream(int rate, int channels);
void SND_QueueStream(std::span<const std::int16_t> samples);
void SND_CloseStream();
// The sound's functions for the modules, the same object for the whole session: it calls the sound in use, or does
// nothing when the sound is off.
sound::Api& SND_Api();
