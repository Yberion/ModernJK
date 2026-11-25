# The device

OpenAL Soft's device and its context, set once for every sound: the output chosen by its name (`s_device`) or the system's default playback device, HRTF off (the game mixes for speakers and headphones alike), the resampler bsinc24 (a band-limited sinc of 24 points), distances in the game's units with a straight-line attenuation between two distances that each source sets, Doppler at the speed of sound in game units (343.3 m/s, a player's 64 units taken for 1.8 m: about 12,200 units per second), the whole mix 9 dB down (`kMixGain`, the listener's gain), so that the game follows ASWG-R001, the loudness standard for games at home (a fight at the default settings at -24 ±2 LUFS, GUIDELINES section 11), and OpenAL Soft's limiter on (off by default for a device mixing in floating point), its threshold 2 dB under full scale (a patch of the library, `libs/openal-soft/patches/0004-limiter-threshold.patch`), so that a crowd's peaks are held there instead of clipping, their true peak under ASWG-R001's -1 dBTP.

In the game: the sound goes to the system's default output, or to the output chosen in the sound menu ("Sound device", `s_device`, empty by default); headphones plugged in during a match take the sound at once, without a restart, when they become the default and no output is chosen; the chosen output unplugged, the sound goes to the default and comes back when it is plugged in again; switching to another window silences the game while its sounds go on (`s_muteWhenUnfocused 1`); the volumes are the voices' (`voices/`), the music's (`stream/`) and the effects', the listener's gain being only the mute.

| file | what it does |
|---|---|
| `device.h` | `Device`: `OpenSystem` (the chosen output, or the default device), `OutputNames` (the system's outputs), `SetChosen`, `OpenLoopback` (rendered into memory, for the tests), `Frame`, the mute, `Render`, `ListenerBoost` (the gain that brings a sound at the listener, panned to the centre, to the level of a sound fully to one side, per output mode: `ListenerBoostOf`); `DecideMove`, the output a frame moves to |
| `device.cpp` | the device, its context and its settings; the events OpenAL Soft sends on its own thread when the system's default device changes or an output appears, acted on at the next frame by reopening the device in place (`alcReopenDeviceSOFT`), which keeps every source and buffer; the outputs' names without the prefix OpenAL Soft gives them on Windows ("OpenAL Soft on ") |

The device is followed in three ways: the event of a new default device, the event of an output that appears, and the device in use reported disconnected (`ALC_CONNECTED`), checked at every frame. `DecideMove` gives the output: the chosen one when it is there, else the system's default, followed when it changes; the outputs are listed only when the move depends on whether the chosen one is there. WASAPI, PipeWire, PulseAudio and CoreAudio report the events, ALSA does not (there an output plugged back is found at the next start). `Frame` tells the sound what it found (`DeviceState`: playing, lost, reopened): while lost, the voices are virtual and the streams wait; a reopen is tried once per second, warned once; after any reopen, the voices are mixed again where they have got to, OpenAL Soft having possibly stopped their sources. The mute without the window's focus is the listener's gain, so it applies to every sound and to the music at once.

## Example

```cpp
std::unique_ptr<sound::Device> device = sound::Device::OpenSystem(host, "Headset (USB Audio)");
device->SetMuted(true);  // the window lost its focus: the sounds go on, silent
device->Frame(now);      // at every frame: the chosen output, or a new default device, is taken here
device->SetChosen("");   // back to the system's default at the next frame
```

## Tests

`test/sound/device.cpp`: the listener's boost of every output mode measured again on the loopback device against `ListenerBoostOf`; OpenAL Soft as the sound uses it on the loopback device (HRTF off, bsinc24, the extensions, a sound played to its end, an EAX reverb in an effect slot), then `Device`: bsinc24 found, the mute silencing the output and lifted; `DecideMove` in each case the menu and the system make (the default followed, the chosen output playing, unplugged and plugged back, another one chosen, a loss), the outputs listed only when the move depends on them.
