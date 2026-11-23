# The streams

The sounds that are not decoded whole: the music, a level's dynamic music and the cinematics' sound, each played on a source of its own at the listener, through a queue of buffers the frame fills again as they play.

In the game: the music. A duel's music in the multiplayer (`music/mp/duel`) is a file played and looped. Yavin's swamp in the single player has a dynamic music: exploring plays `swamp_explore`; when enemies attack, `swamp_action` starts at once; when the fight is over, the music waits for a moment of the piece where a transition fits, plays it, and goes back to exploring; dying plays the death music. The sound of the cinematics between the missions is a stream too.

| file | what it does |
|---|---|
| `stream.h`, `stream.cpp` | `Stream`: one source, four buffers of 8,192 frames (0.74 s to 1.5 s queued, so that a long frame does not starve it), filled by a function at every frame; how much it has played |
| `dms.h`, `dms.cpp` | a level's dynamic music read from `ext_data/dms.dat`: its explore, action and boss pieces, their entry markers and their exits, a level using another's entry (`uses`) or another's boss piece (`useboss`); the rules of the changes |
| `music.h`, `music.cpp` | `Music`: a file, its intro then its loop without a gap; or a level's dynamic music, its pieces cross-faded over a second |

The dynamic music:

- its states are explore, action, boss, death and silence; the single player's game asks for one (`MusicState`), the multiplayer's never does, so its dynamic music stays on explore;
- from action to explore, and from explore or action to silence, the change waits for one of the piece's exit times (within 0.3 s) and plays the transition piece the exit names, which leads to the explore piece's marker or to silence; to action, at once, at one of its entry markers (not the same twice in a row); boss and death at once, and neither left once started, but boss for silence;
- a piece loops by fading its start in over its end, two seconds before it; the death piece fades to silence instead;
- entering a piece at a marker seeks into its file: an MP3 gets a table of 256 seek points at its first seek (0.7 ms), so that the seek takes 0.23 ms instead of decoding every frame up to the marker (23 ms for 60 s of music).

A music asked for by name plays as a file when the name has a folder and the file exists (`music/mp/duel`), and is otherwise the dynamic music of the level the name gives (`yavin1`); when the dynamic music is not allowed (`AllowDynamic(false)`), a level's explore piece plays as a file.

## Example

```cpp
sound::Music music(host, resampler);
music.Start("music/mp/duel", "");                 // a file: its intro, looped
music.Start("yavin1", "");                         // a level's dynamic music, on explore
music.SetState(sound::MusicState::Action);         // the game's fight starts
music.Frame(now);                                  // at every frame
```

## Tests

`test/sound/stream.cpp`: the dynamic music read from a text written for the tests (a level using another's entry and another's boss piece, the tracks' files, the exits' tolerance, the entry markers, the rules, the errors), a file played and looped, a dynamic music changing state through its transition, the cinematics' stream.
