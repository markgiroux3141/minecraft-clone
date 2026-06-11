add_executable(game
    src/GameApp.cpp
    src/GameApp.h
    src/Main.cpp
    src/Player.cpp
    src/Player.h
    src/world/Block.cpp
    src/world/Block.h
    src/world/Chunk.h
    src/world/ChunkMesher.cpp
    src/world/ChunkMesher.h
    src/world/Light.h
    src/world/LightEngine.cpp
    src/world/LightEngine.h
    src/world/TerrainGen.cpp
    src/world/TerrainGen.h
    src/world/World.cpp
    src/world/World.h
)

set_target_properties(game PROPERTIES OUTPUT_NAME "Voxcraft")
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE vox::engine fastnoise)
vox_set_target_defaults(game)
