/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// Synthetic round trip of the message codec (synthetic.h): every message is coded by the server side and decoded by
// a client that keeps its own decoded snapshots, and must equal the original field by field (with the header record).
// Says nothing about compression: the trained tables code these values at a high cost, which is the point (every
// symbol of every alphabet must code). Every compiler generates the same messages: with --hash=<value>, the hash
// (FNV-1a 64) of the coded bytes must equal it.
// Usage: netcodec_test_synthetic [--hash=0x...] [streams] [messages per stream] [seed]
#include "fnv.h"
#include "synthetic.h"

#include <fmt/base.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>

using namespace netcodec;

namespace {

struct Stats {
    size_t messages = 0, failures = 0, bytes = 0, maxBytes = 0, snapshots = 0, vehicle = 0, pilot = 0, noBase = 0, baseOld = 0, gamestates = 0, commands = 0,
           downloads = 0, entities = 0;
    uint32_t maxDelta = 0;
    test::Fnv hash;
};

void RunStream(uint64_t seed, size_t nmsg, Stats& st)
{
    synth::Stream s(seed);
    std::vector<uint8_t> out;
    Message dm;
    for (size_t k = 0; k <= nmsg; k++) {
        const uint32_t seq = s.Seq();
        const synth::Candidate c = s.Next(s.Generator());
        HeaderRecord rec;
        st.messages++;
        const bool coded = s.Encode(c, out, rec);
        if (coded) {
            st.bytes += out.size();
            st.maxBytes = std::max(st.maxBytes, out.size());
            st.hash.Feed(out);
        }
        const bool ok = s.Commit(c, coded ? std::span<const uint8_t>(out) : std::span<const uint8_t>(), rec, dm) && coded;
        if (!ok && st.failures++ < 5)
            fmt::println(stderr, "failure: seed {} seq {} delta {}", seed, seq, c.delta);
        for (const Op& o : c.msg.ops) {
            if (o.type == OpType::GameState)
                st.gamestates++;
            if (o.type == OpType::Command)
                st.commands++;
            if (o.type == OpType::Download)
                st.downloads++;
            if (o.type == OpType::Snapshot) {
                st.snapshots++;
                if (!o.snap.vps.empty())
                    st.vehicle++;
                if (!o.snap.vps.empty() && (detail::Load(o.snap.ps, detail::kPlayerEFlagsOffset) & synth::kEfNodraw) != 0)
                    st.pilot++;
                if (c.delta == 0)
                    st.noBase++;
                if (c.delta > 11)
                    st.baseOld++;
                st.maxDelta = std::max(st.maxDelta, c.delta);
                st.entities += o.snap.entities.count;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    int a = 1;
    std::optional<uint64_t> expected;
    if (argc > 1 && std::strncmp(argv[1], "--hash=", 7) == 0)
        expected = std::strtoull(argv[a++] + 7, nullptr, 16);
    const size_t streams = argc > a ? static_cast<size_t>(std::strtoull(argv[a], nullptr, 10)) : 100;
    const size_t nmsg = argc > a + 1 ? static_cast<size_t>(std::strtoull(argv[a + 1], nullptr, 10)) : 1000;
    const uint64_t seed = argc > a + 2 ? std::strtoull(argv[a + 2], nullptr, 10) : 1;
    Stats st;
    for (size_t i = 0; i < streams; i++)
        RunStream((seed * 1'000'003) + i, nmsg, st);
    fmt::println("synthetic round trip: {} streams, {} messages, {} bytes (largest {}): failures {}", streams, st.messages, st.bytes, st.maxBytes, st.failures);
    fmt::println("  snapshots {} (vehicle {}, pilot {}, no base {}, base older than 11 {}, oldest {}), entities {}, gamestates {}, commands {}, downloads {}",
                 st.snapshots, st.vehicle, st.pilot, st.noBase, st.baseOld, st.maxDelta, st.entities, st.gamestates, st.commands, st.downloads);
    if (expected)
        fmt::println("hash of the coded bytes 0x{:016X}, expected 0x{:016X}{}", st.hash.h, *expected, *expected == st.hash.h ? "" : " (MISMATCH)");
    else
        fmt::println("hash of the coded bytes 0x{:016X}", st.hash.h);
    return st.failures == 0 && st.messages != 0 && (!expected || *expected == st.hash.h) ? 0 : 1;
}
