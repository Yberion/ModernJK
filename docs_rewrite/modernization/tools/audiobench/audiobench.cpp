// Plays a busy game's sound with OpenAL Soft or miniaudio, rendered without a device, and times the mixing; or
// reports the latency of each library's default playback device. For the audio library survey of plan step 15
// (docs_rewrite/modernization/surveys/audio-libraries.md, section 7).
//
// audiobench mix <openal|miniaudio> <gamedata> <file.eal> [voices] [seconds] [reverb 0|1] [hrtf 0|1] [out.wav]
// audiobench latency <openal|miniaudio>
// audiobench listen <openal|miniaudio> <gamedata> <file.eal> <out.wav> [reverb 0|1] [hrtf 0|1]
//
// The scene, the same for both: <voices> looping game sounds (saber hums, shots, footsteps, voices) moving on circles
// around the listener at 100 to 1,500 units, with Doppler from their velocity, one in four through a low-pass filter
// (occlusion), plus the multiplayer duel music, not spatialized; positions updated every 10 ms of sound (a 100 Hz
// frame); with the reverb, the voices split over the first three environments of the .eal file, each environment
// a reverb the voices of that environment feed. 48,000 Hz stereo out, 16-bit sources at their own rate.
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include "miniaudio.h"
#include "ma_reverb_node.h"

#include "EalMan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kRate = 48000;
constexpr int kFrame = kRate / 100; // 10 ms
constexpr float kRefDistance = 80.0f;
constexpr float kMaxDistance = 1500.0f;
// Freeverb's wet control against EAX's room and late reverb levels: a scale chosen so that the reverb's tail after
// the last sound of each environment of the listening extract is about as loud as OpenAL Soft's, by measure
constexpr float kFreeverbWet = 1.35f;

struct Pcm {
    std::vector<int16_t> samples;
    int channels = 1;
    int rate = kRate;
    std::size_t Frames() const { return samples.size() / static_cast<std::size_t>(channels); }
};

Pcm Decode(const std::string& path, int channels)
{
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, static_cast<ma_uint32>(channels), 0); // the file's own rate
    ma_uint64 frames = 0;
    void* data = nullptr;
    if (ma_decode_file(path.c_str(), &cfg, &frames, &data) != MA_SUCCESS) {
        std::fprintf(stderr, "cannot decode %s\n", path.c_str());
        std::exit(1);
    }
    Pcm pcm;
    pcm.channels = channels;
    pcm.rate = static_cast<int>(cfg.sampleRate);
    const auto* s = static_cast<const int16_t*>(data);
    pcm.samples.assign(s, s + frames * static_cast<ma_uint64>(channels));
    ma_free(data, nullptr);
    return pcm;
}

struct Scene {
    std::vector<Pcm> sounds; // mono, spatialized
    Pcm music;               // stereo
    std::vector<EAXListenerProperties> envs;
};

Scene LoadScene(const std::string& dir, const std::string& eal)
{
    static const char* const kSounds[] = {
        "sound/weapons/saber/saberhum1.wav", "sound/weapons/blaster/fire.mp3", "sound/player/footsteps/boot1.mp3",
        "sound/weapons/saber/saberhit1.mp3", "sound/weapons/saber/saberblock1.mp3", "sound/chars/kyle/misc/taunt1.mp3",
        "sound/chars/kyle/misc/death1.mp3",  "sound/chars/luke/misc/death1.mp3",
    };
    Scene scene;
    for (const char* s : kSounds) {
        scene.sounds.push_back(Decode(dir + "/" + s, 1));
    }
    scene.music = Decode(dir + "/music/mp/duel.mp3", 2);
    auto man = std::make_unique<EalMan>();
    if (man->LoadDataSet(eal, 0) != 0) {
        std::fprintf(stderr, "cannot load %s\n", eal.c_str());
        std::exit(1);
    }
    EAXListenerProperties p{};
    for (int i = 0; i < 3 && man->GetEnvironmentAttributes(i, p) == 0; ++i) {
        scene.envs.push_back(p);
    }
    return scene;
}

// voice i's place at time t (s): a circle of radius 100 to 1,500 units, one turn in 4 to 12 s
void Place(int i, double t, float pos[3], float vel[3])
{
    const float radius = 100.0f + static_cast<float>((i * 977) % 1400);
    const double period = 4.0 + (i % 9);
    const double w = 2.0 * 3.14159265358979 / period * ((i & 1) ? 1.0 : -1.0);
    const double a = w * t + i;
    pos[0] = radius * static_cast<float>(std::cos(a));
    pos[1] = static_cast<float>((i % 5) * 20 - 40);
    pos[2] = radius * static_cast<float>(std::sin(a));
    vel[0] = -radius * static_cast<float>(w * std::sin(a));
    vel[1] = 0.0f;
    vel[2] = radius * static_cast<float>(w * std::cos(a));
}

float Gain(int32_t mB) { return std::pow(10.0f, static_cast<float>(mB) / 2000.0f); }

void WriteWav(const std::string& path, const std::vector<float>& out)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return;
    }
    const uint32_t bytes = static_cast<uint32_t>(out.size() * 2);
    auto u32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(kRate); u32(kRate * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (float s : out) {
        const auto v = static_cast<int16_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f));
        std::fwrite(&v, 2, 1, f);
    }
    std::fclose(f);
}

// ---------------------------------------------------------------------------------------------------- OpenAL Soft

void SetEaxReverb(ALuint effect, const EAXListenerProperties& e)
{
    auto f = [effect](ALenum p, float v, float lo, float hi) { alEffectf(effect, p, std::clamp(v, lo, hi)); };
    alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    f(AL_EAXREVERB_DENSITY, e.flEnvironmentSize * e.flEnvironmentSize * e.flEnvironmentSize / 16.0f, 0.0f, 1.0f);
    f(AL_EAXREVERB_DIFFUSION, e.flEnvironmentDiffusion, 0.0f, 1.0f);
    f(AL_EAXREVERB_GAIN, Gain(e.lRoom), 0.0f, 1.0f);
    f(AL_EAXREVERB_GAINHF, Gain(e.lRoomHF), 0.0f, 1.0f);
    f(AL_EAXREVERB_GAINLF, Gain(e.lRoomLF), 0.0f, 1.0f);
    f(AL_EAXREVERB_DECAY_TIME, e.flDecayTime, 0.1f, 20.0f);
    f(AL_EAXREVERB_DECAY_HFRATIO, e.flDecayHFRatio, 0.1f, 2.0f);
    f(AL_EAXREVERB_DECAY_LFRATIO, e.flDecayLFRatio, 0.1f, 2.0f);
    f(AL_EAXREVERB_REFLECTIONS_GAIN, Gain(e.lReflections), 0.0f, 3.16f);
    f(AL_EAXREVERB_REFLECTIONS_DELAY, e.flReflectionsDelay, 0.0f, 0.3f);
    const float rp[3] = {e.vReflectionsPan.fX, e.vReflectionsPan.fY, -e.vReflectionsPan.fZ};
    alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, rp);
    f(AL_EAXREVERB_LATE_REVERB_GAIN, Gain(e.lReverb), 0.0f, 10.0f);
    f(AL_EAXREVERB_LATE_REVERB_DELAY, e.flReverbDelay, 0.0f, 0.1f);
    const float lp[3] = {e.vReverbPan.fX, e.vReverbPan.fY, -e.vReverbPan.fZ};
    alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, lp);
    f(AL_EAXREVERB_ECHO_TIME, e.flEchoTime, 0.075f, 0.25f);
    f(AL_EAXREVERB_ECHO_DEPTH, e.flEchoDepth, 0.0f, 1.0f);
    f(AL_EAXREVERB_MODULATION_TIME, e.flModulationTime, 0.04f, 4.0f);
    f(AL_EAXREVERB_MODULATION_DEPTH, e.flModulationDepth, 0.0f, 1.0f);
    f(AL_EAXREVERB_AIR_ABSORPTION_GAINHF, 1.0f, 0.892f, 1.0f); // the engine set the air absorption to 0
    f(AL_EAXREVERB_HFREFERENCE, e.flHFReference, 1000.0f, 20000.0f);
    f(AL_EAXREVERB_LFREFERENCE, e.flLFReference, 20.0f, 1000.0f);
    f(AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, e.flRoomRolloffFactor, 0.0f, 10.0f);
    alEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT, (e.ulFlags & 0x20u) ? AL_TRUE : AL_FALSE);
}

std::vector<float> MixOpenAL(const Scene& sc, int voices, int frames, bool reverb, bool hrtf, double& seconds)
{
    ALCdevice* dev = alcLoopbackOpenDeviceSOFT(nullptr);
    const ALCint attrs[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT,
                            ALC_FREQUENCY, kRate, ALC_HRTF_SOFT, hrtf ? ALC_TRUE : ALC_FALSE,
                            ALC_MONO_SOURCES, voices + 1, ALC_MAX_AUXILIARY_SENDS, 1, 0};
    ALCcontext* ctx = alcCreateContext(dev, attrs);
    alcMakeContextCurrent(ctx);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    alSpeedOfSound(343.3f * 39.37f); // units are about an inch

    std::vector<ALuint> buffers(sc.sounds.size());
    alGenBuffers(static_cast<ALsizei>(buffers.size()), buffers.data());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        alBufferData(buffers[i], AL_FORMAT_MONO16, sc.sounds[i].samples.data(),
                     static_cast<ALsizei>(sc.sounds[i].samples.size() * 2), sc.sounds[i].rate);
    }
    ALuint musicBuf = 0;
    alGenBuffers(1, &musicBuf);
    alBufferData(musicBuf, AL_FORMAT_STEREO16, sc.music.samples.data(),
                 static_cast<ALsizei>(sc.music.samples.size() * 2), sc.music.rate);

    ALuint slots[3] = {};
    ALuint effects[3] = {};
    const int nenv = reverb ? static_cast<int>(sc.envs.size()) : 0;
    if (nenv > 0) {
        alGenEffects(nenv, effects);
        alGenAuxiliaryEffectSlots(nenv, slots);
        for (int i = 0; i < nenv; ++i) {
            SetEaxReverb(effects[i], sc.envs[static_cast<std::size_t>(i)]);
            alAuxiliaryEffectSloti(slots[i], AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effects[i]));
        }
    }
    ALuint lowpass = 0;
    alGenFilters(1, &lowpass);
    alFilteri(lowpass, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
    alFilterf(lowpass, AL_LOWPASS_GAIN, 0.7f);
    alFilterf(lowpass, AL_LOWPASS_GAINHF, 0.25f);

    std::vector<ALuint> src(static_cast<std::size_t>(voices));
    alGenSources(voices, src.data());
    for (int i = 0; i < voices; ++i) {
        const ALuint s = src[static_cast<std::size_t>(i)];
        alSourcei(s, AL_BUFFER, static_cast<ALint>(buffers[static_cast<std::size_t>(i) % buffers.size()]));
        alSourcei(s, AL_LOOPING, AL_TRUE);
        alSourcef(s, AL_REFERENCE_DISTANCE, kRefDistance);
        alSourcef(s, AL_MAX_DISTANCE, kMaxDistance);
        alSourcef(s, AL_GAIN, 0.5f);
        if (i % 4 == 3) {
            alSourcei(s, AL_DIRECT_FILTER, static_cast<ALint>(lowpass));
        }
        if (nenv > 0) {
            alSource3i(s, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(slots[i % nenv]), 0, AL_FILTER_NULL);
        }
        alSourcePlay(s);
    }
    ALuint music = 0;
    alGenSources(1, &music);
    alSourcei(music, AL_BUFFER, static_cast<ALint>(musicBuf));
    alSourcei(music, AL_LOOPING, AL_TRUE);
    alSourcei(music, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(music, AL_GAIN, 0.25f);
    alSourcePlay(music);

    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    const auto t0 = std::chrono::steady_clock::now();
    for (int done = 0; done < frames; done += kFrame) {
        const double t = static_cast<double>(done) / kRate;
        for (int i = 0; i < voices; ++i) {
            float p[3], v[3];
            Place(i, t, p, v);
            alSourcefv(src[static_cast<std::size_t>(i)], AL_POSITION, p);
            alSourcefv(src[static_cast<std::size_t>(i)], AL_VELOCITY, v);
        }
        const int n = std::min(kFrame, frames - done);
        alcRenderSamplesSOFT(dev, out.data() + static_cast<std::size_t>(done) * 2, n);
    }
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    return out;
}

// ------------------------------------------------------------------------------------------------------ miniaudio

// One environment: a group, not spatialized itself, into a splitter whose second output goes through a Freeverb
// node. ma_reverb_node_init (0.11.26) does not apply its config's controls, so they are set on verblib directly.
void InitEnvironment(ma_engine* engine, const EAXListenerProperties& e, ma_sound_group& group,
                     ma_splitter_node& splitter, ma_reverb_node& reverb)
{
    ma_node_graph* graph = ma_engine_get_node_graph(engine);
    ma_node* endpoint = ma_engine_get_endpoint(engine);
    ma_sound_group_init(engine, 0, nullptr, &group);
    ma_sound_group_set_spatialization_enabled(&group, MA_FALSE);
    ma_splitter_node_config scfg = ma_splitter_node_config_init(2);
    ma_splitter_node_init(graph, &scfg, nullptr, &splitter);
    ma_reverb_node_config rcfg = ma_reverb_node_config_init(2, kRate);
    ma_reverb_node_init(graph, &rcfg, nullptr, &reverb);
    // Freeverb's controls do not hold EAX's: an approximation from the decay time and the levels
    verblib_set_room_size(&reverb.reverb, std::clamp(e.flDecayTime / 4.0f, 0.1f, 0.95f));
    verblib_set_damping(&reverb.reverb, std::clamp(1.0f - e.flDecayHFRatio, 0.0f, 1.0f));
    verblib_set_wet(&reverb.reverb, std::clamp(kFreeverbWet * Gain(e.lRoom) * Gain(e.lReverb), 0.0f, 1.0f));
    verblib_set_dry(&reverb.reverb, 0.0f);
    ma_node_attach_output_bus(&group, 0, &splitter, 0);
    ma_node_attach_output_bus(&splitter, 0, endpoint, 0);
    ma_node_attach_output_bus(&splitter, 1, &reverb, 0);
    ma_node_attach_output_bus(&reverb, 0, endpoint, 0);
}

std::vector<float> MixMiniaudio(const Scene& sc, int voices, int frames, bool reverb, double& seconds)
{
    ma_engine_config ecfg = ma_engine_config_init();
    ecfg.noDevice = MA_TRUE;
    ecfg.channels = 2;
    ecfg.sampleRate = kRate;
    auto engine = std::make_unique<ma_engine>();
    ma_engine_init(&ecfg, engine.get());
    ma_node_graph* graph = ma_engine_get_node_graph(engine.get());
    ma_node* endpoint = ma_engine_get_endpoint(engine.get());

    // the reverb: per environment a group that feeds a splitter, whose second output goes through the reverb
    const int nenv = reverb ? static_cast<int>(sc.envs.size()) : 0;
    std::vector<ma_sound_group> groups(static_cast<std::size_t>(std::max(nenv, 1)));
    std::vector<ma_splitter_node> splitters(groups.size());
    std::vector<ma_reverb_node> reverbs(groups.size());
    for (int i = 0; i < nenv; ++i) {
        const auto k = static_cast<std::size_t>(i);
        InitEnvironment(engine.get(), sc.envs[k], groups[k], splitters[k], reverbs[k]);
    }

    std::vector<ma_audio_buffer_ref> refs(static_cast<std::size_t>(voices) + 1);
    std::vector<ma_sound> sounds(static_cast<std::size_t>(voices) + 1);
    std::vector<ma_lpf_node> lpfs(static_cast<std::size_t>(voices));
    for (int i = 0; i < voices; ++i) {
        const auto k = static_cast<std::size_t>(i);
        const Pcm& pcm = sc.sounds[k % sc.sounds.size()];
        ma_audio_buffer_ref_init(ma_format_s16, 1, pcm.samples.data(), pcm.Frames(), &refs[k]);
        refs[k].sampleRate = static_cast<ma_uint32>(pcm.rate);
        ma_sound_group* group = nenv > 0 ? &groups[static_cast<std::size_t>(i % nenv)] : nullptr;
        ma_sound_init_from_data_source(engine.get(), &refs[k], MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, nullptr,
                                       &sounds[k]);
        ma_node* next = group ? static_cast<ma_node*>(group) : endpoint;
        if (i % 4 == 3) {
            ma_lpf_node_config lcfg = ma_lpf_node_config_init(2, kRate, 2500.0, 2);
            ma_lpf_node_init(graph, &lcfg, nullptr, &lpfs[k]);
            ma_node_set_output_bus_volume(&lpfs[k], 0, 0.7f);
            ma_node_attach_output_bus(&lpfs[k], 0, next, 0);
            next = &lpfs[k];
        }
        ma_node_attach_output_bus(&sounds[k], 0, next, 0);
        ma_sound_set_looping(&sounds[k], MA_TRUE);
        ma_sound_set_attenuation_model(&sounds[k], ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&sounds[k], kRefDistance);
        ma_sound_set_max_distance(&sounds[k], kMaxDistance);
        ma_sound_set_volume(&sounds[k], 0.5f);
        ma_sound_start(&sounds[k]);
    }
    const auto m = static_cast<std::size_t>(voices);
    ma_audio_buffer_ref_init(ma_format_s16, 2, sc.music.samples.data(), sc.music.Frames(), &refs[m]);
    refs[m].sampleRate = static_cast<ma_uint32>(sc.music.rate);
    ma_sound_init_from_data_source(engine.get(), &refs[m], 0, nullptr, &sounds[m]);
    ma_sound_set_spatialization_enabled(&sounds[m], MA_FALSE);
    ma_sound_set_looping(&sounds[m], MA_TRUE);
    ma_sound_set_volume(&sounds[m], 0.25f);
    ma_sound_start(&sounds[m]);

    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    const auto t0 = std::chrono::steady_clock::now();
    for (int done = 0; done < frames; done += kFrame) {
        const double t = static_cast<double>(done) / kRate;
        for (int i = 0; i < voices; ++i) {
            float p[3], v[3];
            Place(i, t, p, v);
            ma_sound_set_position(&sounds[static_cast<std::size_t>(i)], p[0], p[1], p[2]);
            ma_sound_set_velocity(&sounds[static_cast<std::size_t>(i)], v[0], v[1], v[2]);
        }
        const int n = std::min(kFrame, frames - done);
        ma_engine_read_pcm_frames(engine.get(), out.data() + static_cast<std::size_t>(done) * 2,
                                  static_cast<ma_uint64>(n), nullptr);
    }
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    for (auto& s : sounds) {
        ma_sound_uninit(&s);
    }
    ma_engine_uninit(engine.get());
    return out;
}

// --------------------------------------------------------------------------------------------- listening extract

// For listening, not timing: no music, single sounds spaced so that the reverb's tail is heard, the first three
// environments of the .eal file one after the other, 7 s each, every sound sent to that environment's reverb.
struct Event {
    double t;   // within the environment's 7 s
    int sound;  // index in LoadScene's list
    float x, z; // place, in front of the listener (-z)
};
constexpr Event kEvents[] = {
    {0.3, 1, 250.0f, -300.0f}, {1.6, 3, -200.0f, -150.0f}, {2.9, 4, 150.0f, -100.0f},
    {4.0, 5, 0.0f, -250.0f},   {5.6, 1, -300.0f, -200.0f},
};
constexpr double kSegment = 7.0;

std::vector<float> ListenOpenAL(const Scene& sc, bool reverb, bool hrtf)
{
    ALCdevice* dev = alcLoopbackOpenDeviceSOFT(nullptr);
    const ALCint attrs[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT,
                            ALC_FREQUENCY, kRate, ALC_HRTF_SOFT, hrtf ? ALC_TRUE : ALC_FALSE,
                            ALC_MAX_AUXILIARY_SENDS, 1, 0};
    ALCcontext* ctx = alcCreateContext(dev, attrs);
    alcMakeContextCurrent(ctx);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    std::vector<ALuint> buffers(sc.sounds.size());
    alGenBuffers(static_cast<ALsizei>(buffers.size()), buffers.data());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        alBufferData(buffers[i], AL_FORMAT_MONO16, sc.sounds[i].samples.data(),
                     static_cast<ALsizei>(sc.sounds[i].samples.size() * 2), sc.sounds[i].rate);
    }
    const int nenv = static_cast<int>(sc.envs.size());
    ALuint slots[3] = {};
    ALuint effects[3] = {};
    if (reverb) {
        alGenEffects(nenv, effects);
        alGenAuxiliaryEffectSlots(nenv, slots);
        for (int i = 0; i < nenv; ++i) {
            SetEaxReverb(effects[i], sc.envs[static_cast<std::size_t>(i)]);
            alAuxiliaryEffectSloti(slots[i], AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effects[i]));
        }
    }
    ALuint pool[8] = {};
    alGenSources(8, pool);
    int next = 0;
    const int frames = static_cast<int>(kSegment * nenv * kRate);
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int done = 0; done < frames; done += kFrame) {
        const double t = static_cast<double>(done) / kRate;
        const int env = static_cast<int>(t / kSegment);
        const double local = t - env * kSegment;
        for (const Event& e : kEvents) {
            if (e.t >= local && e.t < local + static_cast<double>(kFrame) / kRate) {
                const ALuint s = pool[next++ % 8];
                alSourceStop(s);
                alSourcei(s, AL_BUFFER, static_cast<ALint>(buffers[static_cast<std::size_t>(e.sound)]));
                alSource3f(s, AL_POSITION, e.x, 0.0f, e.z);
                alSourcef(s, AL_REFERENCE_DISTANCE, kRefDistance);
                alSourcef(s, AL_MAX_DISTANCE, kMaxDistance);
                if (reverb) {
                    alSource3i(s, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(slots[env]), 0, AL_FILTER_NULL);
                }
                alSourcePlay(s);
            }
        }
        alcRenderSamplesSOFT(dev, out.data() + static_cast<std::size_t>(done) * 2, std::min(kFrame, frames - done));
    }
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    return out;
}

std::vector<float> ListenMiniaudio(const Scene& sc, bool reverb)
{
    ma_engine_config ecfg = ma_engine_config_init();
    ecfg.noDevice = MA_TRUE;
    ecfg.channels = 2;
    ecfg.sampleRate = kRate;
    auto engine = std::make_unique<ma_engine>();
    ma_engine_init(&ecfg, engine.get());
    ma_node_graph* graph = ma_engine_get_node_graph(engine.get());
    ma_node* endpoint = ma_engine_get_endpoint(engine.get());
    const int nenv = static_cast<int>(sc.envs.size());
    std::vector<ma_sound_group> groups(static_cast<std::size_t>(nenv));
    std::vector<ma_splitter_node> splitters(groups.size());
    std::vector<ma_reverb_node> reverbs(groups.size());
    for (int i = 0; reverb && i < nenv; ++i) {
        const auto k = static_cast<std::size_t>(i);
        InitEnvironment(engine.get(), sc.envs[k], groups[k], splitters[k], reverbs[k]);
    }
    std::vector<std::unique_ptr<ma_audio_buffer_ref>> refs;
    std::vector<std::unique_ptr<ma_sound>> sounds;
    const int frames = static_cast<int>(kSegment * nenv * kRate);
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int done = 0; done < frames; done += kFrame) {
        const double t = static_cast<double>(done) / kRate;
        const int env = static_cast<int>(t / kSegment);
        const double local = t - env * kSegment;
        for (const Event& e : kEvents) {
            if (e.t >= local && e.t < local + static_cast<double>(kFrame) / kRate) {
                const Pcm& pcm = sc.sounds[static_cast<std::size_t>(e.sound)];
                refs.push_back(std::make_unique<ma_audio_buffer_ref>());
                sounds.push_back(std::make_unique<ma_sound>());
                ma_audio_buffer_ref_init(ma_format_s16, 1, pcm.samples.data(), pcm.Frames(), refs.back().get());
                refs.back()->sampleRate = static_cast<ma_uint32>(pcm.rate);
                ma_sound_init_from_data_source(engine.get(), refs.back().get(), MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT,
                                               nullptr, sounds.back().get());
                ma_node* to = reverb ? static_cast<ma_node*>(&groups[static_cast<std::size_t>(env)]) : endpoint;
                ma_node_attach_output_bus(sounds.back().get(), 0, to, 0);
                ma_sound_set_attenuation_model(sounds.back().get(), ma_attenuation_model_inverse);
                ma_sound_set_min_distance(sounds.back().get(), kRefDistance);
                ma_sound_set_max_distance(sounds.back().get(), kMaxDistance);
                ma_sound_set_position(sounds.back().get(), e.x, 0.0f, e.z);
                ma_sound_start(sounds.back().get());
            }
        }
        ma_engine_read_pcm_frames(engine.get(), out.data() + static_cast<std::size_t>(done) * 2,
                                  static_cast<ma_uint64>(std::min(kFrame, frames - done)), nullptr);
    }
    for (auto& s : sounds) {
        ma_sound_uninit(s.get());
    }
    ma_engine_uninit(engine.get());
    return out;
}

// -------------------------------------------------------------------------------------------------------- latency

void Latency(const std::string& lib)
{
    if (lib == "openal") {
        ALCdevice* dev = alcOpenDevice(nullptr);
        ALCcontext* ctx = alcCreateContext(dev, nullptr);
        alcMakeContextCurrent(ctx);
        ALuint src = 0;
        ALuint buf = 0;
        std::vector<int16_t> silence(kRate, 0);
        alGenBuffers(1, &buf);
        alBufferData(buf, AL_FORMAT_MONO16, silence.data(), static_cast<ALsizei>(silence.size() * 2), kRate);
        alGenSources(1, &src);
        alSourcei(src, AL_BUFFER, static_cast<ALint>(buf));
        alSourcePlay(src);
        ALCint refresh = 0;
        ALCint freq = 0;
        alcGetIntegerv(dev, ALC_REFRESH, 1, &refresh);
        alcGetIntegerv(dev, ALC_FREQUENCY, 1, &freq);
        ALCint64SOFT clock[2] = {};
        alcGetInteger64vSOFT(dev, ALC_DEVICE_CLOCK_LATENCY_SOFT, 2, clock);
        std::printf("openal: device \"%s\", %d Hz, mixing period %.2f ms (refresh %d), reported latency %.2f ms\n",
                    alcGetString(dev, ALC_ALL_DEVICES_SPECIFIER), freq, refresh ? 1000.0 / refresh : 0.0, refresh,
                    static_cast<double>(clock[1]) / 1e6);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(ctx);
        alcCloseDevice(dev);
        return;
    }
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = kRate;
    cfg.dataCallback = [](ma_device*, void* out, const void*, ma_uint32 n) {
        std::fill_n(static_cast<float*>(out), n * 2, 0.0f);
    };
    ma_device device;
    if (ma_device_init(nullptr, &cfg, &device) != MA_SUCCESS) {
        std::printf("miniaudio: no device\n");
        return;
    }
    ma_device_start(&device);
    const ma_uint32 period = device.playback.internalPeriodSizeInFrames;
    const ma_uint32 periods = device.playback.internalPeriods;
    const ma_uint32 rate = device.playback.internalSampleRate;
    std::printf("miniaudio: backend %s, %u Hz, period %u frames (%.2f ms) x %u periods = %.2f ms of buffer\n",
                ma_get_backend_name(device.pContext->backend), rate, period, 1000.0 * period / rate, periods,
                1000.0 * period * periods / rate);
    ma_device_uninit(&device);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "latency") {
        Latency(argv[2]);
        return 0;
    }
    if (argc >= 6 && std::string(argv[1]) == "listen") {
        // audiobench listen <openal|miniaudio> <gamedata> <file.eal> <out.wav> [reverb 0|1] [hrtf 0|1]
        const Scene sc = LoadScene(argv[3], argv[4]);
        const bool reverb = argc > 6 ? std::atoi(argv[6]) != 0 : true;
        const bool hrtf = argc > 7 ? std::atoi(argv[7]) != 0 : false;
        const std::vector<float> out = std::string(argv[2]) == "openal" ? ListenOpenAL(sc, reverb, hrtf)
                                                                        : ListenMiniaudio(sc, reverb);
        WriteWav(argv[5], out);
        return 0;
    }
    if (argc < 5 || std::string(argv[1]) != "mix") {
        std::fprintf(stderr, "usage: audiobench mix <openal|miniaudio> <gamedata> <file.eal> [voices] [seconds] "
                             "[reverb 0|1] [hrtf 0|1] [out.wav]\n       audiobench latency <openal|miniaudio>\n");
        return 1;
    }
    const std::string lib = argv[2];
    const int voices = argc > 5 ? std::atoi(argv[5]) : 32;
    const double secs = argc > 6 ? std::atof(argv[6]) : 60.0;
    const bool reverb = argc > 7 ? std::atoi(argv[7]) != 0 : true;
    const bool hrtf = argc > 8 ? std::atoi(argv[8]) != 0 : false;
    const Scene sc = LoadScene(argv[3], argv[4]);
    const int frames = static_cast<int>(secs * kRate);
    double best = 1e30;
    std::vector<float> out;
    for (int run = 0; run < 5; ++run) {
        double t = 0.0;
        out = lib == "openal" ? MixOpenAL(sc, voices, frames, reverb, hrtf, t)
                              : MixMiniaudio(sc, voices, frames, reverb, t);
        best = std::min(best, t);
    }
    double peak = 0.0;
    for (float s : out) {
        peak = std::max(peak, static_cast<double>(std::fabs(s)));
    }
    std::printf("%s: %d voices, reverb %d (%zu environments), hrtf %d: %.1f s of sound mixed in %.3f s (best of 5), "
                "%.3f %% of a core, peak %.2f\n",
                lib.c_str(), voices, reverb ? 1 : 0, reverb ? sc.envs.size() : 0, hrtf ? 1 : 0, secs, best,
                100.0 * best / secs, peak);
    if (argc > 9) {
        WriteWav(argv[9], out);
    }
    return 0;
}
