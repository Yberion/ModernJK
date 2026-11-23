# The ambient sets

The ambient sets of `sound/sound.txt`, which the levels name: a looping sound and one-shots chosen at random.

In the game: Yavin's jungle around the player, its loop and a creature's call every 10 to 50 s (the general set `yavin_jungle`); a computer console humming and beeping as the player comes within 300 units of it (the local set `kejim_console`); a door's start, move and stop (the brush model's set `impdoor1`).

| file | what it does |
|---|---|
| `sets.h`, `sets.cpp` | the sets read from the file, those the level asked for only, their sounds registered: a general set (`generalSet`), a local set (`localSet`, with a radius), a brush model's set (`bmodelSet`, a door's or a platform's sounds by stage) |
| `ambient.h`, `ambient.cpp` | `Ambient`: the general set around the listener, faded over a second into the next one when it changes; the local sets at their entities, louder as the listener comes within their radius; the sets the game no longer asks for stopped |

A local set that goes out of the game's calls for a while and comes back at its entity keeps the time of its next one-shot. A set's one-shot plays when its delay has passed, at a volume drawn within the set's range scaled by the set's own volume; the delay before the next one is drawn once, when one starts, between the set's two times. The old mixer drew it again at every frame, so that the one-shots came at the shortest delay, a little sooner at a higher frame rate (Yavin's jungle's creatures' calls, 10 to 50 s, about every 10 s); they now come at the times the file gives.

The file's lines: `type ambientSet` in its header; a line `generalSet name`, `localSet name` or `bmodelSet name` starts a set, the lines after it give its keywords (`timeBetweenWaves`, `subWaves`, `loopedWave`, `volRange`, and `radius` for a local set; `subWaves` alone for a brush model's set), a comment (`;`) ends it.

## Example

```cpp
ambient.ClearPrecached();
ambient.Precache("yavin_jungle");             // the level's sets, from its entities
ambient.Parse();
// at every frame
ambient.UpdateGeneral("yavin_jungle");
ambient.AddLocal("kejim_console", entity, origin);
ambient.Frame(now, listenerOrigin);
const sound::SoundHandle start = ambient.BrushModelSound("impdoor1", sound::BrushModelStage::Start);
```

## Tests

`test/sound/ambient.cpp`: the sets of a file written for the tests (the sets asked for only, a missing one warned, a comment ending a set, a keyword a kind does not take), a brush model's sounds by stage, a general set's one-shots at the delay drawn once and its loop started once, a local set, the sets stopped when no longer asked for.
