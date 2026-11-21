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

// FNV-1a 64 hash of the determinism checks: values are fed as little-endian bytes.
#pragma once

#include <cstdint>
#include <span>

namespace netcodec::test {

struct Fnv {
	uint64_t h = 0xCBF29CE484222325ULL;
	void Feed(uint64_t v, int bytes) noexcept {
		for (int i = 0; i < bytes; i++) {
			h = (h ^ (v & 0xFFU)) * 0x100000001B3ULL;
			v >>= 8U;
		}
	}
	void Feed(std::span<const uint8_t> bytes) noexcept {
		for (const uint8_t b : bytes) h = (h ^ b) * 0x100000001B3ULL;
	}
};

} // namespace netcodec::test
