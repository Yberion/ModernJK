// No decoder: the harness alone, the base the code size of each decoder is measured against.
#include "bench.h"

static bool Run( const uint8_t *, size_t, Pcm & ) { return false; }
DECODER_EXPORT( "none", Run )
