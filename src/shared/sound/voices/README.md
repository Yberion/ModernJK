# The voices

Every sound played is a voice. A voice is real when OpenAL Soft mixes it on one of the 64 sources, virtual otherwise; a virtual voice's playing position advances all the same, so that it plays on from there when it is mixed again, and a one-shot ends at its own time whether it was heard or not. A sound that starts far from the listener and comes closer is heard from where it has got to; a fight that starts more sounds than 64 keeps the loudest mixed instead of cutting the oldest.

In the game: in a fight of ten players, every hum, clash, footstep, shot and force power is a voice, and the 64 loudest where the player stands are heard. A rocket fired at the player rises in pitch as it comes and falls as it passes (Doppler). A character's line is cut by their pain, and their mouth follows the pain (the voice slot); a weapon's charge is cut by its shot (the weapon slot). The player's own footsteps and shots are heard centred. Under water, everything is muffled.

| file | what it does |
|---|---|
| `classes.h` | what each class gives its sounds: the attenuation (full volume up to a distance, silent from another, a straight line between), whether it is positioned, its volume group (effects or voices), its priority, the volume of the listener's own sounds; the names the log writes |
| `voices.h`, `voices.cpp` | `Voices`: `Play`, `Stop`, `MoveLoop`, `StopSlot`, `StopEntity`, the entities' positions and velocities, the listener, `Frame` (the choice of the voices mixed), `GetVoiceState` (the lip sync), the sound log |

The rules:

- **Choice**: at every frame, the audible voices sorted by their class's priority, then by their loudness at the listener (their volume, their volume group, their class's attenuation at their distance); the first 64 mixed, four of one sound at most, the quieter ones of that sound made virtual.
- **Slots**: a sound started in an entity's slot stops the one playing there: an entity's voice (a new line or a pain cuts the last, the lip sync follows it), its weapon (the shot cuts the charge), a speaker's item; the announcer's and the menus' slots are one for the whole game.
- **Duplicates**: the same sound started again at the same place before the next frame is one voice at the sum of the volumes (the game's data starts some sounds twice to make them louder).
- **The listener's own sounds** are not positioned, at 0.75 of their volume for the classes of the body, the weapons and the voices, as the game always played them.
- **Doppler**: OpenAL Soft's, from the velocities the modules give with `UpdateEntity` and `MoveLoop` and the listener's.
- **Under water**: every sound heard in the water goes through a low-pass filter, -30 dB at 5 kHz, as the original game's EAX occlusion did.
- **A lost device**: every voice is virtual until a device opens again, then mixed where its clock says it has got to.
- **Handles**: an index and a serial number unique in the session (carried over a restart of the sound), so that a handle kept after its voice ended finds nothing.
- **The cost**: a source is given only the values that changed since the last frame (8.9 µs per frame with 264 voices moving, 64 mixed, against 22.8 µs sending everything).
- **The log** (`SetLog`, the engines' `s_show 1`): one `SNDLOG` line per sound started, replaced, merged and ended, and every 50 frames the list of the voices, real or virtual.

| class | full volume up to, silent from (units) | volume | priority |
|---|---|---|---|
| `Voice` | 768, 2,018 | voices | 1 |
| `VoiceQuiet` | 346, 596 | voices | 1 |
| `VoiceEverywhere`, `Announcer` | not positioned | voices | 1 |
| `Interface` | not positioned | effects | 1 |
| `Weapon`, `Saber` | 256, 1,506 | effects | 2 |
| `Force` | 256, 1,506 | effects | 3 |
| `Vehicle` | 2,048, 3,298 | effects | 3 |
| `World` | 256, 1,506 | effects | 4 |
| `Body` | 256, 1,506 | effects | 5 |
| `Ambient` | 256, 1,506 | effects | 6 |

## Example

```cpp
// a pain in the entity's voice slot: the line it was saying stops, its mouth follows the pain
sound::Request pain;
pain.sound = painSound;
pain.soundClass = sound::SoundClass::Voice;
pain.entity = 12;
pain.slot = sound::Slot::Voice;
voices.Play(pain);
voices.UpdateEntity(12, origin, velocity); // at every frame, where it is and how fast it moves
voices.Frame(now);
if (voices.GetVoiceState(12) != sound::VoiceState::NotTalking) {
    // the face talks
}
```

## Tests

`test/sound/voices.cpp`: a sound played to its end, four voices of one sound mixed at most, 64 of 80, a sound out of range virtual then mixed, a slot, the duplicates, the lip sync's state, the stops, the mute, the master volume over the voices, the filter under water, the log.
