# The sound

The sound of every engine of the project, the multiplayer and the single player of Jedi Academy and Jedi Outcast, on OpenAL Soft: one static library, `modernjk_sound`, independent of the engines. An engine gives it the game's files and takes its messages through a `Host` (`host.h`), creates one `System` (`system.h`) and calls it at every frame; every game module (cgame, game, UI) receives the same `System` as an `Api` (`sound.h`) through its import table.

## In the game

Every sound a player hears goes through it: a blaster's shot and the saber's hum, the footsteps on metal, a character's line with the mouth moving to it, the menus' clicks, the announcer's countdown, the music that turns tense when a fight starts in the single player, the creatures' calls of Yavin's jungle, the sound of a cinematic between two missions.

## Folders

| folder | what it does | README |
|---|---|---|
| `decoder/` | the files' decoders: WAV, MP3, FLAC, Ogg Vorbis, chosen by the file's content; a library of its own, `modernjk_sound_decoder`, which the fuzzer takes without OpenAL Soft | [decoder/README.md](decoder/README.md) |
| `device/` | OpenAL Soft's device and context: the output chosen by its name (`s_device`) or the system's default device, followed, HRTF off, the resampler bsinc24, the mute without the focus, the mix's gain after ASWG-R001 (-24 LUFS at the default settings) and the limiter | [device/README.md](device/README.md) |
| `bank/` | the registered sounds: a name given a handle, its file found and decoded whole into an OpenAL buffer, the lip sync's loudness measured | [bank/README.md](bank/README.md) |
| `voices/` | the sounds playing: the classes and the slots, the 64 real voices chosen at every frame and the virtual ones, the attenuation, Doppler, the listener, the voices' state for the lip sync, the sound log | [voices/README.md](voices/README.md) |
| `stream/` | the sounds streamed: the music, a level's dynamic music, the cinematics' sound | [stream/README.md](stream/README.md) |
| `ambient/` | the ambient sets of `sound/sound.txt`: around the listener, at an entity, a door's sounds | [ambient/README.md](ambient/README.md) |
| `zones/` | a map's reverb by zones (`eagle/<map>.efxzones`): the listener's environment and its neighbours in three effect slots, each sound sent to its zone's reverb and muffled by the walls and pillars between, the water, `s_reverb`; the file's reader and the zones' calculations a library of their own, `modernjk_sound_zones`, without OpenAL Soft | [zones/README.md](zones/README.md) |
| `fuzz/` | the libFuzzer targets (preset `windows-clang-fuzz`): `sound_fuzz_decode` (the decoders on any file), `sound_fuzz_text` (the readers of `ext_data/dms.dat`, `sound/sound.txt` and the zone files, which come from the players' archives), `sound_fuzz_system` (the whole sound driven by any sequence of calls, on the loopback device) | |

The files at the root: `sound.h` (what the modules see: the requests, the classes, the slots, the handles, `Api`), `host.h` (what the sound asks of its program), `names.h` (a sound's name without its extension, its file found among the formats from the best to the least good), `system.h` (everything in one object).

## A frame

1. The game modules play and move their sounds through the `Api`: `Play`, `UpdateEntity`, `SetListener`, `MoveLoop`, `StopSlot`, `UpdateAmbientSet`...
2. The engine calls `System::Frame(now)` once, `now` its clock in milliseconds.
3. `Frame` lets the ambient sets start their one-shots and keep their loops, fills the music's and the cinematics' streams, lets the zones find the listener's zone and set their slots, then lets the voices take their sends and filters from the zones and choose the 64 they mix (by their class's priority, then by how loud they are at the listener, four of one sound at most), update every source, and apply it all at once; the device moves to the system's default device if it changed.
4. OpenAL Soft mixes on its own thread until the next frame.

## Examples

A module plays a shot from a weapon, which replaces the charge playing in the same entity's weapon slot:

```cpp
sound::Request shot;
shot.sound = cgs.media.blasterShot;          // a handle from sound->Register("sound/weapons/blaster/fire")
shot.soundClass = sound::SoundClass::Weapon;
shot.place = sound::Place::Entity;           // follows the entity as UpdateEntity moves it
shot.entity = cent->currentState.number;
shot.slot = sound::Slot::Weapon;
sound->Play(shot);
```

A looping sound is started once, kept by its handle, and stopped:

```cpp
sound::Request hum;
hum.sound = saberHum;
hum.soundClass = sound::SoundClass::Saber;
hum.entity = clientNum;
hum.loop = true;
cent->humVoice = sound->Play(hum);
// ... later, when the saber goes off
sound->Stop(cent->humVoice);
```

An engine runs it:

```cpp
sound::Host host{.readFile = ReadGameFile, .fileExists = GameFileExists, .print = PrintLine, .random = Random};
std::unique_ptr<sound::System> system = sound::System::Create(host);
// at every frame
system->SetVolumes(effectsVolume, voiceVolume, musicVolume);
system->Frame(milliseconds);
```

## Tests

`modernjk.sound` in `ctest` (`test/sound`): every part on OpenAL Soft's loopback device, which renders into memory without a sound card, with the sample files of `test/sound/data`; one test file per folder (`decoder.cpp`, `device.cpp`, `bank.cpp`, `voices.cpp`, `stream.cpp`, `ambient.cpp`, `zones.cpp`).

## Rules

The code follows the C++ Core Guidelines entirely, clang-tidy at zero under `.clang-tidy`, the strict warnings of netcodec, a comment on every function, a description at the top of every file, a blank line around each block (`GUIDELINES.md` sections 3, 5 and 10). Its math is LLVM libc's, as netcodec's, not the engines' `q_libm.h`.
