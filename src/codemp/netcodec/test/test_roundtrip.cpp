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

// Round-trip test of the message codec: every message the server loop codes is decoded by a client that keeps its
// own decoded snapshots as bases, and must equal the original field by field (exact 32-bit patterns).
// Each stream is run twice: as recorded, then with every float field that is zero set to -0.0 (the recordings come
// from the legacy coding, which sends -0.0 as 0, so they hold none).
// Determinism: the coded bytes of every message (both runs) are hashed (FNV-1a 64); with --hash=<value>, the hash
// must equal it, so every compiler, architecture and build type codes the corpus to the same bytes (the position and
// angle predictions use floating point). The value changes with the format, the tables or the traces: the test prints
// the new one.
// Usage: netcodec_test_roundtrip [--hash=0x...] <trace.bin> ...   (captures.bin, holdout.bin, ...)
#include "demo_trace.h"
#include "fnv.h"
#include "message_compare.h"
#include "negate_zeros.h"

#include <fmt/base.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <utility>

using namespace netcodec;

namespace {

using trace::ClientStore;

struct Totals {
    size_t messages = 0, encodeFailures = 0, decodeFailures = 0, bytes = 0;
    test::Fnv hash;
};

void RoundTrip(const trace::DemoStream& s, const char* label, Totals& t)
{
    MessageEncoder enc;
    std::vector<uint8_t> out;
    ClientStore client;
    trace::BaselineData clientBaselines;
    trace::DecodeTarget target;
    trace::CommandLog clientHistory;
    Message dm;
    t.encodeFailures += trace::ServerLoop(
        s, [&](const trace::DemoMessage& m, const Base& base, const BaselineTable& baselines, const CommandHistory& history, HeaderRecord& rec) {
            t.messages++;
            if (!enc.Encode(m.msg, base, baselines, history, 1 << 20, out, rec))
                return false;
            t.bytes += out.size();
            t.hash.Feed(out);
            uint32_t delta = 0;
            HeaderRecord drec;
            client.seq = m.seq;
            const bool ok = DecodeMessage(out, client, clientHistory, clientBaselines.View(), clientBaselines.Out(), target.Out(), dm, delta, drec);
            if (!ok || delta != m.delta || !trace::SameMessage(dm, m.msg)) {
                if (t.decodeFailures++ < 5)
                    fmt::println(stderr, "round trip failure: {} {} seq {}{}", s.Meta(1), label, m.seq, ok ? "" : " (decoder failed)");
            }
            if (ok) {
                clientHistory.Add(dm);
                if (const Snapshot* sn = trace::SnapshotOf(dm))
                    client.Keep(m.seq, *sn, drec);
            }
            client.Trim(m.seq);
            return true;
        });
}

} // namespace

int main(int argc, char** argv)
{
    int first = 1;
    std::optional<uint64_t> expected;
    if (argc > 1 && std::strncmp(argv[1], "--hash=", 7) == 0)
        expected = std::strtoull(argv[first++] + 7, nullptr, 16);
    if (argc <= first) {
        fmt::println(stderr, "usage: {} [--hash=0x...] trace.bin ...", argv[0]);
        return 2;
    }
    Totals recorded, negated;
    size_t streams = 0;
    for (int a = first; a < argc; a++) {
        trace::DemoTraceFile in(argv[a]);
        if (!in.Ok())
            return 2;
        trace::DemoStream s;
        while (in.Next(s)) {
            RoundTrip(s, "", recorded);
            NegateZeros(s);
            RoundTrip(s, "(-0.0)", negated);
            streams++;
        }
        if (!in.Ok())
            return 2;
    }
    for (const auto& [label, t] :
         {std::pair<const char*, const Totals&>{"recorded", recorded}, std::pair<const char*, const Totals&>{"zero floats as -0.0", negated}})
        fmt::println("round trip, {}: {} streams, {} messages, {} bytes: encode failures {}, decode failures {}", label, streams, t.messages, t.bytes,
                     t.encodeFailures, t.decodeFailures);
    test::Fnv both;
    both.Feed(recorded.hash.h, 8);
    both.Feed(negated.hash.h, 8);
    if (expected)
        fmt::println("hash of the coded bytes 0x{:016X}, expected 0x{:016X}{}", both.h, *expected, *expected == both.h ? "" : " (MISMATCH)");
    else
        fmt::println("hash of the coded bytes 0x{:016X}", both.h);
    const bool ok =
        (!expected || *expected == both.h) && recorded.encodeFailures + recorded.decodeFailures + negated.encodeFailures + negated.decodeFailures == 0;
    return ok && recorded.messages != 0 ? 0 : 1;
}
