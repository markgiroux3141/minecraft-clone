add_executable(game
    src/GameApp.cpp
    src/GameApp.h
    src/Main.cpp
    src/Player.cpp
    src/Player.h
    src/ui/Hud.cpp
    src/ui/Hud.h
    src/ui/PauseMenu.cpp
    src/ui/PauseMenu.h
    src/ui/TitleScreen.cpp
    src/ui/TitleScreen.h
    src/ui/Widgets.cpp
    src/ui/Widgets.h
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
    src/world/WorldSave.cpp
    src/world/WorldSave.h
)

# Worker-side number crunching (light BFS, greedy meshing, noise) is
# unusably slow at /Od during edit bursts — keep it optimized even in
# Debug builds. Game logic stays fully debuggable.
if(MSVC)
    set_source_files_properties(
        src/world/ChunkMesher.cpp
        src/world/LightEngine.cpp
        src/world/TerrainGen.cpp
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:/O2>")
endif()

set_target_properties(game PROPERTIES OUTPUT_NAME "Voxcraft")
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE vox::engine fastnoise)
vox_set_target_defaults(game)

# Round-trip regression test for the save format (run manually).
add_executable(savetest
    tests/SaveTest.cpp
    src/world/WorldSave.cpp
)
target_include_directories(savetest PRIVATE src)
target_link_libraries(savetest PRIVATE vox::engine)
vox_set_target_defaults(savetest)

# Structural regression test for terrain gen, tree decoration, and the
# packed mesh format (run manually).
add_executable(gentest
    tests/GenTest.cpp
    src/world/Block.cpp
    src/world/ChunkMesher.cpp
    src/world/TerrainGen.cpp
)
target_include_directories(gentest PRIVATE src)
target_link_libraries(gentest PRIVATE vox::engine fastnoise)
vox_set_target_defaults(gentest)
