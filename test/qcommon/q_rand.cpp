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

// The tests of the streams of src/shared/qcommon/q_rand.h seeded from one fixed seed (Rand_InitFixed, which the
// engine's com_debugSeed gives every module): each stream's first values against values computed in Python from the
// published algorithms, not from this code (SplitMix64 seeding of the stream's number above the seed's 32 bits, then
// xoshiro128++), so that the same numbers come on every compiler and platform; the game's stream the same as
// Rand_Init's, which the scenarios' recorded counts depend on; for one seed, every stream different from the others; a
// stream seeded again repeating itself.
// Usage: modernjk_test_rand; prints each failed check, exits with their count.
#include <shared/qcommon/q_rand.h>

#include <check.h>

#include <fmt/base.h>

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::array kStreams = {randStream_t::RAND_GAME, randStream_t::RAND_ENGINE, randStream_t::RAND_CGAME, randStream_t::RAND_UI,
                                 randStream_t::RAND_RENDERER};

using Values = std::array<uint32_t, 4>;

// the module stream's next four values, whole: Q_irand over the whole 32-bit range returns the value as drawn
Values NextValues()
{
    Values values{};
    for (uint32_t& value : values) {
        value = static_cast<uint32_t>(Q_irand(INT_MIN, INT_MAX));
    }
    return values;
}

Values FixedValues(int seed, randStream_t stream)
{
    Rand_InitFixed(seed, stream);
    return NextValues();
}

// the first four values of each stream for the seeds 7 and -1, computed in Python from the published algorithms
void TestKnownValues()
{
    constexpr std::array<Values, 5> seven = {{
        {0x18576505u, 0x0e6be122u, 0x5fca7e0eu, 0x55070afdu},
        {0x92f2a9f8u, 0xf4330a77u, 0xffd324b0u, 0xa6a95676u},
        {0x94bea079u, 0x8833ae52u, 0xc920c2ccu, 0x823371f6u},
        {0x19171cb8u, 0x216f6070u, 0x6170efb1u, 0x779c8e0cu},
        {0x00e404b6u, 0x7416650eu, 0x108b6443u, 0xb8ae5facu},
    }};

    for (std::size_t i = 0; i < kStreams.size(); ++i) {
        CHECK(FixedValues(7, kStreams[i]) == seven[i]);
    }
    CHECK((FixedValues(-1, randStream_t::RAND_GAME) == Values{0x38d3e448u, 0x5079f70bu, 0x1182cdb7u, 0x97b5b8b8u}));
    CHECK((FixedValues(-1, randStream_t::RAND_RENDERER) == Values{0xa5afd1a4u, 0x29c6e38cu, 0x7ab4fe65u, 0x5499dbd6u}));
}

// the game's stream from a fixed seed is Rand_Init's from that seed, negative seeds included
void TestGameStreamIsRandInit()
{
    for (const int seed : {0, 1, 7, -1, 123'456'789, INT_MIN, INT_MAX}) {
        Rand_Init(seed);
        const Values fromRandInit = NextValues();
        CHECK(FixedValues(seed, randStream_t::RAND_GAME) == fromRandInit);
    }
}

// for one seed, no two streams start with the same value
void TestStreamsDiffer()
{
    for (int seed = -50; seed <= 50; ++seed) {
        std::array<uint32_t, kStreams.size()> first{};
        for (std::size_t i = 0; i < kStreams.size(); ++i) {
            first[i] = FixedValues(seed, kStreams[i])[0];
        }
        for (std::size_t i = 0; i < first.size(); ++i) {
            for (std::size_t j = i + 1; j < first.size(); ++j) {
                CHECK_NE(first[i], first[j]);
            }
        }
    }
}

// a stream seeded again from the same seed draws the same values again, after other draws in between
void TestSeededAgainRepeats()
{
    for (const randStream_t stream : kStreams) {
        const Values first = FixedValues(42, stream);
        (void)NextValues();
        CHECK(FixedValues(42, stream) == first);
    }
}

} // namespace

int main()
{
    TestKnownValues();
    TestGameStreamIsRandInit();
    TestStreamsDiffer();
    TestSeededAgainRepeats();
    fmt::println("{} failed check(s)", ::test::failures);
    return ::test::failures;
}
