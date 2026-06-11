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
