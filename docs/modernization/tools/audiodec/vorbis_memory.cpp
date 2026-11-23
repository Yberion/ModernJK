// The memory stb_vorbis needs to open and decode each file of a corpus (sample_vorbis.bin): setup, setup's temporary
// and decoding's temporary memory, as stb_vorbis_get_info reports them; prints the largest of each and of their sum
// (the size of the block src/shared/sound/decoder/vorbis.cpp gives stb_vorbis comes from it).
// build: clang++ -O2 -std=c++20 -w -I<repository>/libs/stb vorbis_memory.cpp -o vorbis_memory
// usage: vorbis_memory <build/audiolibs/decoders/data/sample_vorbis.bin>
#include "stb_vorbis.c"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    FILE* f = std::fopen(argv[1], "rb");
    char magic[4];
    uint32_t count = 0;
    if (!f || std::fread(magic, 1, 4, f) != 4 || std::fread(&count, 4, 1, f) != 1)
        return 1;
    unsigned setup = 0, setupTemp = 0, temp = 0, sum = 0;
    std::string worst;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t nl = 0;
        uint32_t dl = 0;
        std::string name;
        if (std::fread(&nl, 2, 1, f) != 1)
            return 1;
        name.resize(nl);
        if (std::fread(name.data(), 1, nl, f) != nl || std::fread(&dl, 4, 1, f) != 1)
            return 1;
        std::vector<unsigned char> data(dl);
        if (std::fread(data.data(), 1, dl, f) != dl)
            return 1;
        int error = 0;
        stb_vorbis* v = stb_vorbis_open_memory(data.data(), int(dl), &error, nullptr);
        if (!v)
            continue;
        const stb_vorbis_info info = stb_vorbis_get_info(v);
        setup = info.setup_memory_required > setup ? info.setup_memory_required : setup;
        setupTemp = info.setup_temp_memory_required > setupTemp ? info.setup_temp_memory_required : setupTemp;
        temp = info.temp_memory_required > temp ? info.temp_memory_required : temp;
        const unsigned s = info.setup_memory_required + info.setup_temp_memory_required + info.temp_memory_required;
        if (s > sum) {
            sum = s;
            worst = name;
        }
        stb_vorbis_close(v);
    }
    std::printf("files %u: setup %u, setup temp %u, temp %u bytes at most; the largest sum %u (%s)\n", count, setup, setupTemp, temp, sum, worst.c_str());
    return 0;
}
