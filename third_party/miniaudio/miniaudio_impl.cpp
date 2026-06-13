// miniaudio implementation, compiled exactly once for the whole build (ODR
// guard). vox::AudioEngine includes "miniaudio.h" without MA_IMPLEMENTATION,
// so the symbols live only here.
//
// We decode Ogg Vorbis ourselves via stb_vorbis (compiled as its own C TU in
// this same static lib) and feed the PCM to miniaudio through ma_audio_buffer,
// so miniaudio's built-in file decoders are unused — MA_NO_ENCODING trims the
// write path we never touch.
#define MA_IMPLEMENTATION
#define MA_NO_ENCODING
#include "miniaudio.h"
