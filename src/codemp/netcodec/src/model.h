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

// netcodec internals: the model, one table per context (contexts.h), built once: the trained frequencies of
// generated/jka_tables.inc (server messages) and generated/jka_client_tables.inc (client packets), and for every other
// context the table of zero counts (uniform), shared by alphabet size.
#pragma once

#include <netcodec/rans.h>

#include <array>
#include <vector>

namespace netcodec::detail {

class Model {
public:
    [[nodiscard]] const rans::Table& Table(uint32_t ctx) const noexcept { return tables_[ctx]; }
    [[nodiscard]] int8_t TrTypeBits() const noexcept { return trTypeBits_; } // width of the entity field pos.trType

    [[nodiscard]] static const Model& Get(); // built on first use (thread-safe), shared by every coder

private:
    Model();

    std::vector<uint16_t> cum_;
    std::vector<rans::EncSymbol> enc_;
    std::vector<std::array<rans::CoarseEntry, rans::kCoarseSize>> coarse_; // one per distinct table, plus one for empty contexts
    std::vector<rans::Table> tables_;                                      // per context
    int8_t trTypeBits_ = 8;
};

} // namespace netcodec::detail
