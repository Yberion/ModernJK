/*
===========================================================================
Copyright (C) 2015 - 2016, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

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

#include "qcommon/safe/string.h"

#include "check.h"

namespace string_tests
{

void literals()
{
	auto test = CSTRING_VIEW( "test" );
	auto foo = CSTRING_VIEW( "foo" );

	CHECK_EQUAL( test, CSTRING_VIEW( "test" ) );
	CHECK_EQUAL( test.size(), 4 );

	CHECK_EQUAL( foo, CSTRING_VIEW( "foo" ) );
	CHECK_EQUAL( foo.size(), 3 );
	CHECK_NE( test, foo );
}

void stricmp()
{
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "hello" ), CSTRING_VIEW( "HELLO" ) ), Q::Ordering::EQ );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "aaa" ), CSTRING_VIEW( "aab" ) ), Q::Ordering::LT );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "aab" ), CSTRING_VIEW( "aaa" ) ), Q::Ordering::GT );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "AAA" ), CSTRING_VIEW( "aab" ) ), Q::Ordering::LT );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "AAA" ), CSTRING_VIEW( "aab" ) ), Q::Ordering::LT );
	// prefix is smaller
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "hello" ), CSTRING_VIEW( "hello world" ) ), Q::Ordering::LT );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "hello world" ), CSTRING_VIEW( "hello" ) ), Q::Ordering::GT );
	// edge case: empty strings
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "" ), CSTRING_VIEW( "" ) ), Q::Ordering::EQ );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "" ), CSTRING_VIEW( "a" ) ), Q::Ordering::LT );
	CHECK_EQUAL( Q::stricmp( CSTRING_VIEW( "a" ), CSTRING_VIEW( "" ) ), Q::Ordering::GT );
}

void substr()
{
	CHECK_EQUAL( Q::substr( CSTRING_VIEW( "Hello World" ), 6 ), CSTRING_VIEW( "World" ) );
	CHECK_EQUAL( Q::substr( CSTRING_VIEW( "Hello World" ), 6, 100 ), CSTRING_VIEW( "World" ) );
	CHECK_EQUAL( Q::substr( CSTRING_VIEW( "Hello World" ), 0, 5 ), CSTRING_VIEW( "Hello" ) );
	CHECK_EQUAL( Q::substr( CSTRING_VIEW( "Hello my World!" ), 6, 2 ), CSTRING_VIEW( "my" ) );
	CHECK_THROW( Q::substr( CSTRING_VIEW( "Hello" ), 20 ), std::out_of_range );
}

void svtoi()
{
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "" ) ), 0 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "asdf" ) ), 0 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "0" ) ), 0 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "1" ) ), 1 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "+1" ) ), 1 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "-1" ) ), -1 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "13foo" ) ), 13 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "    13" ) ), 13 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "13 27" ) ), 13 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "12345" ) ), 12345 );
	CHECK_EQUAL( Q::svtoi( CSTRING_VIEW( "-12345" ) ), -12345 );
}

void sscanf()
{
	{
		float x = -1, y = -1, z = -1;
		CHECK_EQUAL( Q::sscanf( CSTRING_VIEW( "  42  13.37" ), x, y, z ), 2 );
		CHECK_EQUAL( x, 42.f );
		CHECK_CLOSE_FRACTION( y, 13.37f, 0.1f ); // a relative difference of at most 0.1
		CHECK_EQUAL( z, -1 );
	}
	// TODO String tests
}

} // namespace string_tests

void RunStringTests()
{
	string_tests::literals();
	string_tests::stricmp();
	string_tests::substr();
	string_tests::svtoi();
	string_tests::sscanf();
}
