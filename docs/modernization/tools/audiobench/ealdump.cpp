// Reads the game's EAX zone files (eagle/*.eal) with EalMan, built as EaxMan 3.0 (EAXMANVERSION=3), and prints what
// the engine's EAX path took from them: the environments (named, with their main reverb values), the apertures
// between them, and a check that the centre point of every environment is found in that environment.
// Usage: ealdump <file.eal>...
#include "EalMan.h"

#include <cstdio>
#include <memory>
#include <string>

namespace {

int Count(const EalMan& eal, const char* name)
{
    int32_t id = -1;
    int32_t n = 0;
    if (eal.GetSourceID(name, id) != 0 || eal.GetSourceNumInstances(id, n) != 0) {
        return -1;
    }
    return n;
}

void Dump(const char* path, bool verbose)
{
    auto eal = std::make_unique<EalMan>();
    const int32_t rc = eal->LoadDataSet(path, 0);
    if (rc != 0) {
        std::printf("%s: load failed 0x%08X\n", path, static_cast<unsigned>(rc));
        return;
    }
    const int envs = Count(*eal, "Center");
    int apertures = 0;
    for (int i = 0; Count(*eal, ("Aperture" + std::to_string(i) + "a").c_str()) > 0; ++i) {
        apertures += Count(*eal, ("Aperture" + std::to_string(i) + "a").c_str());
    }

    // the engine takes environment i's centre as instance i of "Center" and asks which environment holds it
    int centresOk = 0;
    int32_t centreId = -1;
    eal->GetSourceID("Center", centreId);
    for (int i = 0; i < envs; ++i) {
        EMPoint p{};
        int32_t env = -2;
        if (eal->GetSourceInstancePos(centreId, i, p) == 0 &&
            eal->GetListenerDynamicAttributes(0, p, env, 0) == 0 && env >= 0) {
            ++centresOk;
        }
    }
    // a file without "Center" points is played by the engine with one reverb, the listener's environment
    int presets = 0;
    for (EAXListenerProperties p{}; presets < 256 && eal->GetEnvironmentAttributes(presets, p) == 0;) {
        ++presets;
    }
    std::printf("%s: environments %d, apertures %d, centres found in an environment %d of %d, presets %d\n", path,
                envs, apertures, centresOk, envs, presets);
    if (!verbose) {
        return;
    }
    for (int i = 0; i < presets; ++i) {
        EAXListenerProperties p{};
        std::string name;
        if (eal->GetEnvironmentAttributes(i, p) != 0) {
            continue;
        }
        eal->GetEnvironmentName(i, name, 256);
        std::printf("  env %2d %-24s preset %2u size %6.2f m, room %6d mB, decay %5.2f s, HF ratio %4.2f, "
                    "reflections %6d mB at %5.3f s, reverb %6d mB at %5.3f s\n",
                    i, name.c_str(), p.ulEnvironment, p.flEnvironmentSize, p.lRoom, p.flDecayTime,
                    p.flDecayHFRatio, p.lReflections, p.flReflectionsDelay, p.lReverb, p.flReverbDelay);
    }
}

} // namespace

int main(int argc, char** argv)
{
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-v") {
            verbose = true;
            continue;
        }
        Dump(argv[i], verbose);
    }
    return 0;
}
