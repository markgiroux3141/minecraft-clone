#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vox/audio/AudioEngine.h"
#include "world/Block.h" // SoundType

namespace vc {

// Owns every loaded sound clip and maps game events to vox::AudioEngine calls.
// One instance lives on GameApp. All clips load from the gitignored
// assets/mc/sounds/ overlay (zero-distribution rule); a clean clone with no
// overlay loads invalid handles, so every trigger below is a silent no-op.
//
// Sound model follows vanilla 1.7.10: dig/break/place share the per-material
// "dig.<mat>" clips (break/place louder + lower-pitched, the mining tick
// quieter), footsteps use "step.<mat>", and a handful of one-offs (pop,
// splash, fire loop, cave ambient, music) round it out.
class GameSounds {
public:
    // Loads every clip. Call once from GameApp::OnInit after audio.Init().
    void Load(vox::AudioEngine& audio);

    // --- Block interaction (positional, at the block center) ---------------
    void PlayDig(SoundType type, const glm::vec3& blockCenter);   // per mining tick
    void PlayBreak(SoundType type, const glm::vec3& blockCenter); // on destroy
    void PlayPlace(SoundType type, const glm::vec3& blockCenter);
    void PlayPickup(); // random/pop, 2D (item vacuumed into the bag)
    // M26 buckets: item/bucket/{fill,empty}[_lava]N. `lava` picks the lava
    // variant (water otherwise).
    void PlayBucketFill(bool lava, const glm::vec3& pos);
    void PlayBucketEmpty(bool lava, const glm::vec3& pos);

    // --- Player locomotion -------------------------------------------------
    void PlayStep(SoundType type, const glm::vec3& feet);
    void PlayLand(SoundType type, const glm::vec3& feet);
    void PlaySplash(const glm::vec3& pos);
    void PlayHurt(); // M30: the player took damage (2D, randomly pitched)

    // --- M32 mobs (positional; hostile picks the zombie set, else pig) ------
    void PlayMobHurt(bool hostile, const glm::vec3& pos);
    void PlayMobDeath(bool hostile, const glm::vec3& pos);

    // --- Looping furnace voice (GameApp reconciles these against lit furnaces) -
    vox::VoiceHandle StartFurnaceLoop(const glm::vec3& blockCenter);
    void StopFurnaceLoop(vox::VoiceHandle& voice);

    // --- Ambient / music schedulers (call once per frame while in a world) -
    void UpdateAmbient(bool inDarkness, double frameDt, const glm::vec3& listenerPos);
    void UpdateMusic(double frameDt);

private:
    static constexpr int kMaxVariants = 18; // cave ambient has 18 in 1.12
    struct ClipSet {
        std::array<vox::ClipHandle, kMaxVariants> clips{};
        int count = 0;
    };

    // Index by SoundType (cast to int); Glass falls back to the stone set for
    // dig/step/place (it only has a distinct break sound).
    static constexpr int kSoundTypeCount = 9;

    ClipSet LoadSet(const char* folder, const char* material, int maxVariants);
    const ClipSet& DigSet(SoundType type) const;
    const ClipSet& StepSet(SoundType type) const;
    vox::ClipHandle Pick(const ClipSet& set);
    float Jitter(float base, float spread); // base * (1 +/- spread)

    vox::AudioEngine* m_audio = nullptr;

    std::array<ClipSet, kSoundTypeCount> m_dig{};
    std::array<ClipSet, kSoundTypeCount> m_step{};
    ClipSet m_glassBreak{}; // random/glass*
    ClipSet m_splash{};     // liquid/splash*
    ClipSet m_bucketFill{};      // item/bucket/fill* (water)
    ClipSet m_bucketFillLava{};  // item/bucket/fill_lava*
    ClipSet m_bucketEmpty{};     // item/bucket/empty* (water)
    ClipSet m_bucketEmptyLava{}; // item/bucket/empty_lava*
    ClipSet m_caveAmbient{};// ambient/cave/cave*
    ClipSet m_hurt{};       // damage/hit* (player hurt)
    // M32 mob voices (mob/pig, mob/zombie). Pig has no distinct hurt clip in
    // 1.12 (it reuses "say"), so PlayMobHurt falls back to the say set.
    ClipSet m_pigSay{};
    ClipSet m_pigDeath{};
    ClipSet m_zombieSay{};
    ClipSet m_zombieHurt{};
    ClipSet m_zombieDeath{};
    vox::ClipHandle m_pop{};
    vox::ClipHandle m_fireLoop{};
    // Music is decoded on demand (one track at a time), so we keep paths, not
    // preloaded clips — see vox::AudioEngine::PlayMusic.
    std::vector<std::filesystem::path> m_musicPaths;

    // Schedulers.
    double m_ambientTimer = 30.0; // seconds of darkness until the next cave sound
    double m_musicGap = 90.0;     // seconds until the first track
    uint32_t m_rng = 0x9e3779b9u;
};

} // namespace vc
