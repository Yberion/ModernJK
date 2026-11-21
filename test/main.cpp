// The tests of src/shared/qcommon/safe (the checked strings and the vector of fixed capacity the single player uses).
// Usage: modernjk_test_safe; prints each failed check, exits with their count.
#include "check.h"

#include <cstdio>

void RunStringTests();
void RunLimitedVectorTests();

int main()
{
	RunStringTests();
	RunLimitedVectorTests();
	std::printf( "%d failed check(s)\n", ::test::failures );
	return ::test::failures;
}
