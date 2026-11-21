// The reference decoder of an executable (REF_MP3CODE, REF_ENGINEWAV, REF_LIBFLAC, REF_VORBISFILE, REF_OPUSFILE):
// the decoder files compiled a second time under the reference's entry points.
#undef DEC_MP3CODE
#undef DEC_MINIMP3
#undef DEC_DRMP3
#undef DEC_MPG123
#undef DEC_ENGINEWAV
#undef DEC_DRWAV
#undef DEC_DRFLAC
#undef DEC_LIBFLAC
#undef DEC_STBVORBIS
#undef DEC_VORBISFILE
#undef DEC_OPUSFILE
#define AS_REFERENCE

#if defined( REF_MP3CODE )
#define DEC_MP3CODE
#include "dec_mp3.cpp"
#elif defined( REF_MPG123 )
#define DEC_MPG123
#include "dec_mp3.cpp"
#elif defined( REF_ENGINEWAV )
#define DEC_ENGINEWAV
#include "dec_wav.cpp"
#elif defined( REF_LIBFLAC )
#define DEC_LIBFLAC
#include "dec_other.cpp"
#elif defined( REF_VORBISFILE )
#define DEC_VORBISFILE
#include "dec_other.cpp"
#elif defined( REF_OPUSFILE )
#define DEC_OPUSFILE
#include "dec_other.cpp"
#endif
