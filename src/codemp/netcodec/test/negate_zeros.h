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

// The corpus streams with every zero float field set to -0.0 (the recordings come from the legacy coding, which sends
// -0.0 as 0, so they hold none): the second run of the round-trip tests. White-box: reads the schema.
#pragma once

#include "demo_trace.h"
#include <schema.h>

#include <span>

namespace netcodec::trace {

inline constexpr uint32_t kNegativeZero = 0x80000000U;

template <size_t N> void NegateZeros(std::array<uint8_t, N>& img, std::span<const detail::FieldDesc> fields)
{
    for (const detail::FieldDesc& f : fields)
        if (f.bits == 0 && detail::Load(img, f.offset) == 0)
            detail::StoreWord(img, f.offset, kNegativeZero);
}

// the player-state table the codec sends: the pilot table for a player in a vehicle with EF_NODRAW
[[nodiscard]] inline bool IsPilot(const PlayerState& ps) noexcept
{
    constexpr uint32_t kEfNodraw = 1U << 8U;
    return detail::ValueBits(detail::Load(ps, detail::kVehicleNumOffset), 10) != 0 && (detail::Load(ps, detail::kPlayerEFlagsOffset) & kEfNodraw) != 0;
}

inline void NegateZeros(SnapshotData& sn)
{
    if (IsPilot(sn.ps))
        NegateZeros(sn.ps, detail::kPilotFields);
    else
        NegateZeros(sn.ps, detail::kPlayerFields);
    if (sn.hasVehicle)
        NegateZeros(sn.vps, detail::kVehicleFields);
    for (EntityState& e : sn.entities)
        NegateZeros(e, detail::kEntityFields);
}

// in the messages' storage (the ops are views of it)
inline void NegateZeros(DemoStream& s)
{
    for (DemoMessage& m : s.messages) {
        if (m.data.snap)
            NegateZeros(*m.data.snap);
        if (m.data.gs)
            for (EntityState& e : m.data.gs->table)
                if (EntityNumber(e) != 0)
                    NegateZeros(e, detail::kEntityFields); // the baselines present only
    }
}

} // namespace netcodec::trace
