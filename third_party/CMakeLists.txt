# glad: OpenGL 4.6 core function loader, pre-generated (see README for regen command)
add_library(glad STATIC glad/src/gl.c)
target_include_directories(glad PUBLIC glad/include)
set_target_properties(glad PROPERTIES FOLDER "third_party")

# stb_image: single-header image loader, implementation compiled here once
add_library(stb STATIC stb/stb_image.cpp)
target_include_directories(stb PUBLIC stb)
set_target_properties(stb PROPERTIES FOLDER "third_party")

# FastNoiseLite: single-header noise library (header-only)
add_library(fastnoise INTERFACE)
target_include_directories(fastnoise INTERFACE fastnoise)

# miniaudio: single-header audio playback + device backend. The implementation
# is compiled once in miniaudio_impl.cpp (ODR guard). stb_vorbis.c is compiled
# as its own C TU here for .ogg decoding (we pre-decode to PCM and feed
# miniaudio via ma_audio_buffer — see vox::AudioEngine).
add_library(miniaudio STATIC miniaudio/miniaudio_impl.cpp miniaudio/stb_vorbis.c)
target_include_directories(miniaudio PUBLIC miniaudio)
set_target_properties(miniaudio PROPERTIES FOLDER "third_party")
if(WIN32)
    # WASAPI/DirectSound backends: miniaudio loads these lazily, but link the
    # core COM/multimedia libs so the static lib resolves cleanly under MSVC.
    target_link_libraries(miniaudio PRIVATE ole32 winmm)
endif()
if(MSVC)
    # stb_vorbis ogg decoding is unusably slow at /Od (multi-second clip loads
    # in Debug). Keep this third-party lib optimized even in Debug — we never
    # step through it. Same rationale as the game's worker files.
    target_compile_options(miniaudio PRIVATE $<$<CONFIG:Debug>:/O2>)
endif()
