# The registered sounds

A sound's name is given a handle once; its file is found, decoded whole, mixed to mono if it is stereo, measured for the lip sync and given to OpenAL Soft as a buffer. The handle stays valid for the whole session: the sounds a level did not register nor play are freed when the next level's registration ends, and loaded again if they are played after (a menu's sound registered once at the start, for instance).

In the game: when a level loads, its sounds are registered: the weapons', the characters' (Kyle's pain, `sound/chars/kyle/misc/pain25.mp3`), the map's. A French player hears the French lines, `sound/chr_f/`. The lip sync is for the dialogue and the mouth's movement: when Kyle says a line, his mouth opens on its loud syllables and closes between its words; a caption stays as long as its line lasts.

| file | what it does |
|---|---|
| `bank.h`, `bank.cpp` | `Bank`: `Register` (a name to a handle), `Use` (a sound to play, loaded again if freed), `Length`, the language of the voices, the levels' use and the freeing |
| `lipsync.h`, `lipsync.cpp` | a sound's loudness, measured once by blocks of about 18 ms, and a voice's state at a frame of its sound (not talking, silent, levels 1 to 4) |

What a name finds:

- the name is keyed in lower case, with `/`, without its extension: `Sound\Weapons\Fire.WAV` and `sound/weapons/fire.mp3` are one sound;
- the file is the first of `.flac`, `.wav` (both lossless), `.ogg`, `.mp3`, from the best format to the least good (`../names.h`); the game's own files are WAV or MP3, so `.wav` comes before `.mp3` as it always did;
- a character's voice (`sound/chars/`) is looked for in the player's language first, `sound/chr_d/`, `chr_f/` or `chr_e/`, then in English;
- each sound has an event: the sounds whose names differ only by a trailing number, the variants the game draws among, are one (`Event`), which the voices count for their limit of four;
- a missing file gives `kNoSound` and one message shown only to a developer (`developer 1`, as the old mixers did), its name never looked for again; a name starting with `*` stands for a character's own sound, which the game resolves before registering it.

The lip sync measures what Jedi Academy's mixer measured at 44,100 Hz: every 800 samples, the mean of ten samples taken 100 apart, each divided by 256 and squared; a voice's level compares its block with the sound's peak by Jedi Academy's thresholds (0.3, 4, 6, 8). The samples themselves are not kept: OpenAL Soft holds the buffer, the bank keeps a few bytes per second of loudness.

## Example

```cpp
sound::Bank bank(host);
const sound::SoundHandle pain = bank.Register("sound/chars/kyle/misc/pain25.wav");
const int milliseconds = bank.Length(pain); // how long a caption stays
bank.BeginLevel();                          // a new level registers its sounds
// ... registration
bank.Free(bank.Unused());                   // what it did not use
```

## Tests

`test/sound/bank.cpp`: the names, the best format first, a stereo file mixed to mono, a missing file reported once, the language, the length, the freeing and the loading again; the lip sync on a signal of silence, then loud, then quiet.
