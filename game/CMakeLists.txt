add_executable(game
    # App shell (root)
    src/GameApp.cpp
    src/GameApp.h
    src/InputState.h
    src/Main.cpp
    # Audio
    src/audio/Sounds.cpp
    src/audio/Sounds.h
    # Entities (simulated bodies + their box-models)
    src/entity/ChickenModel.cpp
    src/entity/ChickenModel.h
    src/entity/CowModel.cpp
    src/entity/CowModel.h
    src/entity/CreeperModel.cpp
    src/entity/CreeperModel.h
    src/entity/Entity.h
    src/entity/EntityManager.cpp
    src/entity/EntityManager.h
    src/entity/Explosion.cpp
    src/entity/HumanoidModel.cpp
    src/entity/HumanoidModel.h
    src/entity/Mob.cpp
    src/entity/Mob.h
    src/entity/MobModel.h
    src/entity/PigModel.cpp
    src/entity/PigModel.h
    src/entity/Player.cpp
    src/entity/Player.h
    src/entity/SheepModel.cpp
    src/entity/SheepModel.h
    # Items / inventory / crafting
    src/item/Crafting.cpp
    src/item/Crafting.h
    src/item/Inventory.cpp
    src/item/Inventory.h
    src/item/Item.cpp
    src/item/Item.h
    # Game-side render helpers
    src/render/Particles.cpp
    src/render/Particles.h
    src/render/PlayerDoll.cpp
    src/render/PlayerDoll.h
    src/render/ViewModel.cpp
    src/render/ViewModel.h
    # UI / screens / HUD
    src/ui/BlockIcons.cpp
    src/ui/BlockIcons.h
    src/ui/Hud.cpp
    src/ui/Hud.h
    src/ui/InventoryScreen.cpp
    src/ui/InventoryScreen.h
    src/ui/PauseMenu.cpp
    src/ui/PauseMenu.h
    src/ui/TitleScreen.cpp
    src/ui/TitleScreen.h
    src/ui/Widgets.cpp
    src/ui/Widgets.h
    # Voxel world (chunks, meshing, lighting, worldgen, blocks, persistence)
    src/world/Block.cpp
    src/world/Block.h
    src/world/CaveGen.cpp
    src/world/CaveGen.h
    src/world/Chunk.h
    src/world/ChunkMesher.cpp
    src/world/ChunkMesher.h
    src/world/Furnace.cpp
    src/world/Furnace.h
    src/world/JavaRandom.h
    src/world/LakeGen.cpp
    src/world/LakeGen.h
    src/world/Light.h
    src/world/LightEngine.cpp
    src/world/LightEngine.h
    src/world/OreGen.cpp
    src/world/OreGen.h
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
        src/world/CaveGen.cpp
        src/world/ChunkMesher.cpp
        src/world/LakeGen.cpp
        src/world/LightEngine.cpp
        src/world/OreGen.cpp
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
    src/world/CaveGen.cpp
    src/world/ChunkMesher.cpp
    src/world/LakeGen.cpp
    src/world/OreGen.cpp
    src/world/TerrainGen.cpp
)
target_include_directories(gentest PRIVATE src)
target_link_libraries(gentest PRIVATE vox::engine fastnoise)
vox_set_target_defaults(gentest)
