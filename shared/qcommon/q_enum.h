/*
===========================================================================
Copyright (C) 2026, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
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

#pragma once

// q_enum.h -- the operations on enumerations, written once (C++ Core Guidelines, Enum.4). Every enumeration is an
// enum class (Enum.3), so that its values convert to an integer only where the code says so: through these
// operations, or with to_underlying where a value leaves for a file, a cvar, a name table or a format string.

#include <cassert>
#include <cstddef>
#include <type_traits>

// C++23's std::to_underlying, until the project moves to C++23
template <typename E>
	requires std::is_enum_v<E>
[[nodiscard]] constexpr std::underlying_type_t<E> to_underlying( E e ) noexcept {
	return static_cast<std::underlying_type_t<E>>( e );
}

// An array indexed by an enumeration, with the size and the layout of T[Count]: `int level[NUM_FORCE_POWERS]`
// becomes `EnumArray<int, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> level`, still indexed `level[forcePowers_t::FP_PUSH]`.
template <typename T, typename E, auto Count>
	requires std::is_enum_v<E>
struct EnumArray {
	static constexpr std::size_t N = static_cast<std::size_t>( Count );
	T elems[N];

	[[nodiscard]] constexpr T &operator[]( E e ) noexcept {
		assert( static_cast<std::size_t>( to_underlying( e ) ) < N );
		return elems[to_underlying( e )];
	}
	[[nodiscard]] constexpr const T &operator[]( E e ) const noexcept {
		assert( static_cast<std::size_t>( to_underlying( e ) ) < N );
		return elems[to_underlying( e )];
	}
	[[nodiscard]] constexpr T *data() noexcept { return elems; }
	[[nodiscard]] constexpr const T *data() const noexcept { return elems; }
	[[nodiscard]] static constexpr std::size_t size() noexcept { return N; }
	[[nodiscard]] constexpr T *begin() noexcept { return elems; }
	[[nodiscard]] constexpr T *end() noexcept { return elems + N; }
	[[nodiscard]] constexpr const T *begin() const noexcept { return elems; }
	[[nodiscard]] constexpr const T *end() const noexcept { return elems + N; }
};

// The arithmetic of an ordered enumeration whose code walks its values or offsets them (`BOTH_A1_T__B_ + quadrant`,
// `for ( weapon_t w = weapon_t::WP_SABER; w < weapon_t::WP_NUM_WEAPONS; w++ )`): ++, --, enumeration + int, enumeration - int, and the
// distance between two values. An enumeration takes them with `template <> inline constexpr bool
// enableEnumArithmetic<E> = true;` after its definition.
template <typename E>
inline constexpr bool enableEnumArithmetic = false;

template <typename E>
concept ArithmeticEnum = std::is_enum_v<E> && enableEnumArithmetic<E>;

template <ArithmeticEnum E>
[[nodiscard]] constexpr E operator+( E e, int n ) noexcept {
	return static_cast<E>( to_underlying( e ) + n );
}
template <ArithmeticEnum E>
[[nodiscard]] constexpr E operator-( E e, int n ) noexcept {
	return static_cast<E>( to_underlying( e ) - n );
}
template <ArithmeticEnum E>
[[nodiscard]] constexpr int operator-( E a, E b ) noexcept {
	return to_underlying( a ) - to_underlying( b );
}
template <ArithmeticEnum E>
constexpr E &operator+=( E &e, int n ) noexcept {
	return e = e + n;
}
template <ArithmeticEnum E>
constexpr E &operator-=( E &e, int n ) noexcept {
	return e = e - n;
}
template <ArithmeticEnum E>
constexpr E &operator++( E &e ) noexcept {
	return e = e + 1;
}
template <ArithmeticEnum E>
constexpr E &operator--( E &e ) noexcept {
	return e = e - 1;
}
template <ArithmeticEnum E>
constexpr E operator++( E &e, int ) noexcept {
	const E old = e;
	e = e + 1;
	return old;
}
template <ArithmeticEnum E>
constexpr E operator--( E &e, int ) noexcept {
	const E old = e;
	e = e - 1;
	return old;
}

// The operators of a flag enumeration, whose values are bits (C++ Core Guidelines I.4, Enum.8): |, &, ^, ~ and their
// assignments, and Any( flags & X ) for the test `if ( flags & X )`: at least one bit of X. An enumeration takes them
// with `template <> inline constexpr bool enableEnumFlags<E> = true;` after its definition.
template <typename E>
inline constexpr bool enableEnumFlags = false;

template <typename E>
concept FlagEnum = std::is_enum_v<E> && enableEnumFlags<E>;

template <FlagEnum E>
[[nodiscard]] constexpr E operator|( E a, E b ) noexcept {
	return static_cast<E>( to_underlying( a ) | to_underlying( b ) );
}
template <FlagEnum E>
[[nodiscard]] constexpr E operator&( E a, E b ) noexcept {
	return static_cast<E>( to_underlying( a ) & to_underlying( b ) );
}
template <FlagEnum E>
[[nodiscard]] constexpr E operator^( E a, E b ) noexcept {
	return static_cast<E>( to_underlying( a ) ^ to_underlying( b ) );
}
template <FlagEnum E>
[[nodiscard]] constexpr E operator~( E a ) noexcept {
	return static_cast<E>( ~to_underlying( a ) );
}
template <FlagEnum E>
constexpr E &operator|=( E &a, E b ) noexcept {
	return a = a | b;
}
template <FlagEnum E>
constexpr E &operator&=( E &a, E b ) noexcept {
	return a = a & b;
}
template <FlagEnum E>
constexpr E &operator^=( E &a, E b ) noexcept {
	return a = a ^ b;
}
template <FlagEnum E>
[[nodiscard]] constexpr bool Any( E flags ) noexcept {
	return to_underlying( flags ) != 0;
}

// The bit of an enumerator in an integer mask: `forcePowersKnown & Bit( FP_PUSH )` for `1 << FP_PUSH`
template <typename E>
	requires std::is_enum_v<E>
[[nodiscard]] constexpr int Bit( E e ) noexcept {
	assert( to_underlying( e ) >= 0 && to_underlying( e ) < 32 );
	return 1 << to_underlying( e );
}

// checked by the compiler
namespace enumSelfCheck {
enum class Sample { a, b, c };
enum class Flags { none = 0, x = 1, y = 2 };
}
template <>
inline constexpr bool enableEnumArithmetic<enumSelfCheck::Sample> = true;
template <>
inline constexpr bool enableEnumFlags<enumSelfCheck::Flags> = true;
static_assert( Any( enumSelfCheck::Flags::x | enumSelfCheck::Flags::y ) && !Any( enumSelfCheck::Flags::x & enumSelfCheck::Flags::y ) );
static_assert( to_underlying( ~enumSelfCheck::Flags::x & ( enumSelfCheck::Flags::x | enumSelfCheck::Flags::y ) ) == 2 );
static_assert( Bit( enumSelfCheck::Sample::c ) == 4 );
static_assert( to_underlying( enumSelfCheck::Sample::c ) == 2 );
static_assert( enumSelfCheck::Sample::a + 2 == enumSelfCheck::Sample::c && enumSelfCheck::Sample::c - 1 == enumSelfCheck::Sample::b );
static_assert( enumSelfCheck::Sample::c - enumSelfCheck::Sample::a == 2 );
static_assert( sizeof( EnumArray<int, enumSelfCheck::Sample, enumSelfCheck::Sample::c> ) == sizeof( int[2] ) );
