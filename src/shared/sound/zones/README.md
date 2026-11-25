# The zones

A map's reverb by zones, as the original game's EAX path played it on Creative's sound cards, here through OpenAL Soft's EFX. A map is cut into zones by a BSP of planes, each zone in an environment (a reverb: "Tie Fighter Hall", "Lunch Room"); the listener hears the reverb of the environment it stands in and, on a map whose file gives apertures, of the two environments whose nearest aperture is nearest, each coming from its aperture. Each sound is sent to the reverb of its own zone, and muffled by the obstacles the file puts between zones (walls) and by the diffraction boxes inside one (pillars).

In the game: on `mp/ffa3`, a player in the hall of the TIE fighter hears its long reverb, the lunch room's through its door ahead and the north hall's to the side; a shot fired in the lunch room rings with the lunch room's reverb, coming through the door. On Jedi Outcast's `doom_comm`, a sound behind a wall of glass loses its high frequencies. Walking from one room into another on a map with one reverb, the reverb turns into the next room's in one second. Under water, the "underwater" reverb alone. `s_reverb 0` switches the reverb off; the walls and the water still muffle what is behind them, for every player alike.

| file | what it does |
|---|---|
| `zones.h`, `zones.cpp` | the zone file (`eagle/<map>.efxzones`): its reader and its writer; the zone of a point (the BSP walked down) and a source's obstruction and occlusion (the obstacle between two zones, the diffraction boxes in one), computed as Creative's EAX Manager computed them; a library of its own, `modernjk_sound_zones`, without OpenAL Soft, which the converter, the fuzzer and the tests take |
| `reverb.h`, `reverb.cpp` | `ZoneReverb`: three effect slots holding EFX's "EAX reverb", the listener's zone, the neighbours, the pans, the fade, the water, `s_reverb`; for each sound its send and the gains of its two low-pass filters (`Of`) |

The rules:

- **The file** is written once by `tools/eal_converter` from the game's EAX zone files (`eagle/<map>.eal`), in each game's `z_ModernJK.pk3`; a map without it has no reverb. It keeps the axes of Creative's editor (x as the game's, y up, z the game's y): a game's point is swapped into them, without arithmetic, and the calculations follow EAX Manager's operation by operation, so that the converter finds the same results as EalMan to the bit on every file. A damaged file is refused with its reason (it comes from the players' archives): the limits are far above the game's files, a BSP that loops is walked no longer than its planes.
- **The listener's zone**, at every frame; out of every zone, the reverbs of the last zone it was in.
- **The neighbours** (a file with apertures): the two environments whose nearest aperture is nearest to the listener, chosen again every 100 ms, as the original did, and at once when the listener enters another environment, so that the one it leaves keeps its slot; each slot holds an environment, an emptied slot computes nothing.
- **The pans** (`REFLECTIONSPAN`, `LATEREVERBPAN`), at every frame, in the listener's axes (right, up, forward): a neighbour's towards its aperture, as strong as the chord over the arc of the angle the aperture covers; the listener's own away from its environment's nearest aperture, as strong as one minus that.
- **One reverb** (a file without apertures): the listener's environment in one slot, faded into the next in 1 s by ten steps of 100 ms (the levels linearly in mB, the density and the times on a logarithmic scale, the rest linearly, as Jedi Outcast's original interpolated EAX's values), each step one of OpenAL Soft's own short fades.
- **A sound's send**: its zone's reverb, the listener's when its zone has none in a slot or none at all; a sound at the listener (the listener's own, the ambient sets around it) the listener's reverb; a sound not in the world (the announcer, the interface, the voices heard everywhere) none.
- **The obstacles**: the obstacle between the listener's zone and the sound's (an occlusion muffles the direct sound and the send, an obstruction the direct sound), then in one zone the first diffraction box crossed, as low-pass filters with the conversions of OpenAL Soft's own EAX layer (`al/source.cpp`); a character's voice never, as the original left the voices out. The direct filter's broadband gain lowers the sound's audibility in the choice of the 64 voices mixed.
- **Under water**: every sound in the world through the water's filter (-30 dB at 5 kHz, the low frequencies kept), sent to the "underwater" reverb alone (`EFX_REVERB_PRESET_UNDERWATER`); the zones' obstacles not computed; out of the water, the zones' reverbs again at once.
- **`s_reverb 0`**: the slots emptied, no send; the obstacles' and the water's filters stay, since they change what a player hears of the others.

## Example

```cpp
// a map's zones, the listener at every frame, a shot's send and filters
std::string error;
reverb.Load(sound::ParseZoneFile(text, error));
reverb.SetListener(listener);
reverb.Frame(now);
const sound::VoiceZone zone = reverb.Of(shotPosition, sound::Hearing::InWorld);
// zone.slot: the effect slot its send goes to; zone.directGain and directGainHF, sendGain and sendGainHF: its filters
```

## Tests

`test/sound/zones.cpp`: the file written and read back the same, every truncation of it read or refused, damaged files refused, a cycle in the BSP; the zone of a point and a plane's thickness; a wall's occlusion between two rooms, a pillar's obstruction and none beside it; on the loopback device, the reverb heard after a sound ended and none with the reverb off, a sound behind the wall quieter, the water, a map with one reverb; four rooms in a row: the two nearest neighbours of three, an equal distance, a point out of every zone, a room left keeping its slot, the pans (ahead, below with the view raised, the listener's own away from its door), the fade's third step, the filters' and the sends' gains of every way of hearing, under water too, a sound behind a wall left virtual before 64 as loud, the zones kept through a restart. `sound_fuzz_text` reads its inputs as zone files too. `tools/eal_converter` checks every converted file against EalMan.
