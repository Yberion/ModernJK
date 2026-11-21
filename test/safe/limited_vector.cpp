#include "qcommon/safe/limited_vector.h"

#include <memory>
#include <string>
#include <iterator>

#include "check.h"

using IntVector = Q::LimitedVector< int, 10 >;
using StringVector = Q::LimitedVector< std::string, 10 >;
using IntPtrVector = Q::LimitedVector< std::unique_ptr< int >, 10 >;
using SharedPtrVector = Q::LimitedVector< std::shared_ptr< int >, 10 >;

namespace limited_vector_tests
{

void default_construct()
{
	IntVector intVec;
	CHECK_EQUAL( intVec.size(), 0 );
	CHECK( intVec.begin() == intVec.end() );
	CHECK( intVec.empty() );
}

void fill_and_copy()
{
	StringVector stringVec;
	CHECK_EQUAL( stringVec.size(), 0 );
	CHECK( stringVec.begin() == stringVec.end() );
	CHECK( stringVec.empty() );

	CHECK( stringVec.push_back( "hello world" ) );
	CHECK( stringVec.emplace_back( "emplaced" ) );

	CHECK_EQUAL( stringVec.size(), 2 );
	CHECK( !stringVec.empty() );
	CHECK_EQUAL( std::distance( stringVec.begin(), stringVec.end() ), 2 );

	auto it = stringVec.begin();
	auto cit = stringVec.cbegin();

	CHECK( it != stringVec.end() );
	CHECK( cit != stringVec.end() );
	CHECK_EQUAL( *it, "hello world" );
	CHECK_EQUAL( *cit, "hello world" );
	CHECK_EQUAL( stringVec[ 0 ], "hello world" );

	++it;
	++cit;

	CHECK( it != stringVec.end() );
	CHECK( cit != stringVec.end() );
	CHECK_EQUAL( *it, "emplaced" );
	CHECK_EQUAL( *cit, "emplaced" );
	CHECK_EQUAL( stringVec[ 1 ], "emplaced" );

	++it;
	++cit;

	CHECK( it == stringVec.end() );
	CHECK( cit == stringVec.end() );

	StringVector stringVecCopy = stringVec;

	CHECK( stringVec == stringVecCopy );
	CHECK( !( stringVec != stringVecCopy ) );
	CHECK_EQUAL( stringVec.size(), 2 );
	CHECK_EQUAL( stringVec.size(), stringVecCopy.size() );
	CHECK_EQUAL( stringVec[ 0 ], stringVecCopy[ 0 ] );
	CHECK_EQUAL( stringVec[ 1 ], stringVecCopy[ 1 ] );

	stringVec.clear();

	CHECK( stringVec.empty() );
	CHECK_EQUAL( stringVec.size(), 0 );
	CHECK( stringVec != stringVecCopy );
	CHECK( !( stringVec == stringVecCopy ) );

	stringVec = stringVecCopy;

	CHECK( stringVec == stringVecCopy );
	CHECK( !( stringVec != stringVecCopy ) );
	CHECK_EQUAL( stringVec.size(), 2 );
	CHECK_EQUAL( stringVec.size(), stringVecCopy.size() );
	CHECK_EQUAL( stringVec[ 0 ], stringVecCopy[ 0 ] );
	CHECK_EQUAL( stringVec[ 1 ], stringVecCopy[ 1 ] );
}

void fill_and_move()
{
	IntPtrVector vec1, vec2;
	CHECK( vec1.push_back( std::unique_ptr< int >( new int(42) ) ) );
	CHECK( vec1.push_back( std::unique_ptr< int >( new int(1337) ) ) );
	CHECK( vec2.push_back( std::unique_ptr< int >( new int(0) ) ) );
	CHECK( vec2.emplace_back() ); // nullptr3

	CHECK_EQUAL( vec1.size(), 2 );
	CHECK_EQUAL( vec2.size(), 2 );
	CHECK_EQUAL( *vec1[ 0 ], 42 );
	CHECK_EQUAL( *vec1[ 1 ], 1337 );
	CHECK_EQUAL( *vec2[ 0 ], 0 );
	CHECK( !vec2[ 1 ] );

	vec1.swap( vec2 );

	CHECK_EQUAL( vec1.size(), 2 );
	CHECK_EQUAL( vec2.size(), 2 );
	CHECK_EQUAL( *vec2[ 0 ], 42 );
	CHECK_EQUAL( *vec2[ 1 ], 1337 );
	CHECK_EQUAL( *vec1[ 0 ], 0 );
	CHECK( !vec1[ 1 ] );

	vec1 = std::move( vec2 );

	CHECK_EQUAL( vec1.size(), 2 );
	CHECK_EQUAL( vec2.size(), 0 );
	CHECK_EQUAL( *vec1[ 0 ], 42 );
	CHECK_EQUAL( *vec1[ 1 ], 1337 );
}

void member_destruction()
{
	// check for proper destruction of members
	std::shared_ptr< int > counter = std::make_shared< int >( 42 );
	{
		CHECK_EQUAL( counter.use_count(), 1 );
		SharedPtrVector vec;
		CHECK( vec.push_back( counter ) );
		CHECK( vec.push_back( counter ) );
		CHECK( vec.push_back( counter ) );
		CHECK_EQUAL( counter.use_count(), 4 );
		vec.pop_back();
		CHECK_EQUAL( counter.use_count(), 3 );
		SharedPtrVector vec2 = vec;
		CHECK_EQUAL( counter.use_count(), 5 );
		vec = std::move( vec2 );
		CHECK_EQUAL( counter.use_count(), 3 );
	}
	CHECK_EQUAL( counter.use_count(), 1 );
}

} // namespace limited_vector_tests

void RunLimitedVectorTests()
{
	limited_vector_tests::default_construct();
	limited_vector_tests::fill_and_copy();
	limited_vector_tests::fill_and_move();
	limited_vector_tests::member_destruction();
}
