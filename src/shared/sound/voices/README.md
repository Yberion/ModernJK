# The voices

Every sound played is a voice. A voice is real when OpenAL Soft mixes it on one of the 64 sources, virtual otherwise; a virtual voice's playing position advances all the same, so that it plays on from there when it is mixed again, and a one-shot ends at its own time whether it was heard or not. A sound that starts far from the listener and comes closer is heard from where it has got to; a fight that starts more sounds than 64 keeps the loudest mixed instead of cutting the oldest.

In the game: in a fight of ten players, every hum, clash, footstep, shot and force power is a voice, and the 64 loudest where the player stands are heard. A rocket fired at the player rises in pitch as it comes and falls as it passes (Doppler). A character's line is cut by their pain, and their mouth follows the pain (the voice slot); a weapon's charge is cut by its shot (the weapon slot). The player's own footsteps and shots are heard centred. Under water, everything is muffled.

| file | what it does |
|---|---|
| `classes.h` | what each class gives its sounds: the attenuation (full volume up to a distance, silent from another, a straight line between), whether it is positioned, its volume group (effects or voices), its priority, the volume of the listener's own sounds, its own gain as a bus's; the names the log writes |
| `voices.h`, `voices.cpp` | `Voices`: `Play`, `Stop`, `MoveLoop`, `StopSlot`, `StopEntity`, the entities' positions and velocities, the listener, `Frame` (the choice of the voices mixed), `GetVoiceState` (the lip sync), the sound log |

The rules:

- **Choice**: at every frame, the audible voices sorted by their class's priority, then by their loudness at the listener (their volume, their volume group, their class's attenuation at their distance); the first 64 mixed by their importance (their audibility weighted by what remains to be heard of them, a voice already mixed raised by a tenth so that two close ones do not swap sources at every frame), four of one event at most among the sounds played once: the variants of a sound (names that differ only by a trailing number, `saberhitwall1` to `3`) count together, a near sound beats a far new one, a fast weapon's new shot the end of an old one; the others virtual.
- **The loops of one sound** are one voice, as the old mixer summed them on one cycle: the copy that holds a source (else the loudest) plays them at the sum of their audibilities, capped at full volume on each ear in front at their class's volume (the listener's boost times the volume setting and the class's gain, as the old mixer capped them before its volume: in stereo 1.678 on the near ear fully to one side, 0.68 behind), from the direction they come from weighted by their audibility, through the send and the high frequencies' filter of the loudest copy, without the broadband filter, each copy's being in the sum (two torches behind one wall muffled once, not twice); eleven torches with the same fire are one fire, no copy echoing another. Every loop starts on one cycle, so that the copy that plays them can change without a jump.
- **Slots**: a sound started in an entity's slot (at the entity or at a point, not at the listener) stops the one playing there: an entity's voice (a new line or a pain cuts the last, the lip sync follows it), its weapon (the shot cuts the charge), a speaker's item; the announcer's and the menus' slots are one for the whole game.
- **Duplicates**: the same sound started again at the same place before the next frame is one voice at the sum of the volumes (the game's data starts some sounds twice to make them louder); in a slot, the second replaces the first.
- **The lip sync** of a voice mixed follows its source's playing position, which plays in real time; a virtual one's follows the frame's clock, which follows `timescale` (the single player's Force Speed).
- **The listener's own sounds** are not positioned, at 0.75 of their volume for the classes of the body, the weapons and the voices, as the game always played them. A sound at the listener or of the listener's own entity is boosted to the level the old mixer gave it, as loud on each ear as a sound fully to one side is on its own (OpenAL Soft pans it to the centre, 0.596 per channel in stereo: `Device::ListenerBoost`, 1.678 in stereo), an ambient set's there to half of it, where the old mixer placed it at the listener's point.
- **Doppler**: OpenAL Soft's, from the velocities the modules give with `UpdateEntity` and `MoveLoop` and the listener's.
- **The zones** (`zones/`): each source's send to a reverb and its two low-pass filters, of its direct sound and of that send, as the zones give them at every frame (the walls, the pillars, the water: -30 dB at 5 kHz under water, as the original game's EAX occlusion did); the direct filter's broadband gain lowers the voice's audibility in the choice.
- **A lost device**: every voice is virtual until a device opens again, then mixed where its clock says it has got to.
- **Handles**: an index and a serial number unique in the session (carried over a restart of the sound), so that a handle kept after its voice ended finds nothing.
- **The cost**: a source, the listener and the streams are given only the values that changed since the last frame (8.9 µs per frame with 264 voices moving, 64 mixed, against 22.8 µs sending everything).
- **The log** (`SetLog`, the engines' `s_show 1`): one `SNDLOG` line per sound started, replaced, merged and ended, and every 50 frames the zones' slots and the list of the voices, real or virtual, each with its send's slot and its direct filter's gains.

| class | full volume up to, silent from (units) | volume | priority | gain |
|---|---|---|---|---|
| `Voice` | 768, 2,018 | voices | 1 | 1 |
| `VoiceQuiet` | 346, 596 | voices | 1 | 1 |
| `VoiceEverywhere` | not positioned | voices | 1 | 1 |
| `Announcer` | not positioned | effects | 1 | 1 |
| `Interface` | not positioned | effects | 1 | 1 |
| `Weapon` | 256, 1,506 | effects | 2 | 1 |
| `Saber` | 256, 1,506 | effects | 2 | 0.79 (-2 dB) |
| `Force` | 256, 1,506 | effects | 3 | 1 |
| `Vehicle` | 2,048, 3,298 | effects | 3 | 1 |
| `World` | 256, 1,506 | effects | 4 | 1 |
| `Body` | 256, 1,506 | effects | 5 | 1 |
| `Ambient` | 256, 1,506 | effects | 6 | 1 |

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

`test/sound/voices.cpp`: a sound played to its end, four voices of one sound mixed at most (the newest shots of a fast weapon, a near sound before a far new one), the loops of one sound one voice at their summed gain (behind a wall, muffled once), 64 of 80, a sound out of range virtual then mixed, a slot, the duplicates, the lip sync's state, with the clock behind the sound too, the stops, the mute, the volumes of the effects and the voices apart, the filter under water, the log.
