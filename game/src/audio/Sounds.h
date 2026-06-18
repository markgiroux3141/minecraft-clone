#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "vox/audio/AudioEngine.h"
#include "world/Block.h" // SoundType

namespace vc {

class World;
enum class MobType : uint8_t; // entity/Mob.h — used by the mob-voice API below

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

    // Footstep cadence + landing thud + water-entry splash, reconciled each
    // tick from the player's feet/ground/water state. `world` resolves the
    // supporting block's material; `moving` gates footsteps (true only while
    // the player has movement input — landing/splash fire regardless). Owns
    // the stride accumulator and edge-tracking state. Call ResetLocomotion on
    // spawn/world-enter so the first tick re-seeds rather than emitting a step.
    void ResetLocomotion();
    void UpdateLocomotion(const World& world, const glm::vec3& feet, bool grounded, bool inWater,
                          bool moving);

    // --- Mining dig cadence (paces the per-material "dig" tick sound) -------
    // Call ResetDigSound when the dig target changes / digging begins so the
    // next TickDigSound fires immediately; TickDigSound advances the ~4-tick
    // cadence and plays PlayDig when it elapses.
    void ResetDigSound();
    void TickDigSound(SoundType type, const glm::vec3& blockCenter, double frameDt);

    // --- Mobs (positional; per-type voice sets, M32 + M34 roster) -----------
    void PlayMobHurt(MobType type, const glm::vec3& pos);
    void PlayMobDeath(MobType type, const glm::vec3& pos);
    void PlaySheepShear(const glm::vec3& pos);  // M34: shears snip
    void PlayChickenEgg(const glm::vec3& pos);  // M34: egg "plop"
    void PlayExplosion(const glm::vec3& pos);    // M35: TNT/creeper boom
    void PlayCreeperPrime(const glm::vec3& pos); // M35: creeper fuse hiss
    void PlayBowShoot(const glm::vec3& pos);     // M36: bow release (player + skeleton)
    void PlayEat(const glm::vec3& pos);          // M37: a chewing crunch (per bite)
    void PlayBurp(const glm::vec3& pos);         // M37: the burp on the last bite

    // --- Looping furnace crackle (one voice per lit furnace) ----------------
    // Reconciles the live voices against the world's currently-lit furnaces:
    // starts new ones, stops those that went out or unloaded. Owns the voice
    // map. StopAllFurnaceLoops tears them all down before the world (and its
    // furnaces) vanish — call from world-exit.
    void ReconcileFurnaceLoops(const World& world);
    void StopAllFurnaceLoops();

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

    vox::VoiceHandle StartFurnaceLoop(const glm::vec3& blockCenter);
    void StopFurnaceLoop(vox::VoiceHandle& voice);

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
    // Per-MobType voices (mob/<folder>/{say,hurt,death}; folder from
    // MobSoundFolder). Some mobs lack a distinct hurt clip in 1.12 (pig, cow,
    // sheep, chicken reuse "say"), so PlayMobHurt falls back to the say set.
    static constexpr int kMobVoiceCount = 7; // == MobType::Count (asserted in .cpp)
    struct MobVoice {
        ClipSet say;
        ClipSet hurt;
        ClipSet death;
    };
    std::array<MobVoice, kMobVoiceCount> m_mobVoices{};
    ClipSet m_sheepShear{}; // mob/sheep/shear*
    ClipSet m_chickenEgg{}; // mob/chicken/plop*
    ClipSet m_explode{};    // random/explode* (M35 TNT/creeper boom)
    ClipSet m_creeperFuse{}; // random/fuse (M35 creeper prime hiss)
    ClipSet m_bow{};         // random/bow (M36 bow release)
    ClipSet m_eat{};         // random/eat1..3 (M37 chewing crunch)
    vox::ClipHandle m_burp{};// random/burp (M37 finished eating)
    vox::ClipHandle m_pop{};
    vox::ClipHandle m_fireLoop{};
    // Music is decoded on demand (one track at a time), so we keep paths, not
    // preloaded clips — see vox::AudioEngine::PlayMusic.
    std::vector<std::filesystem::path> m_musicPaths;

    // Furnace crackle: one looping voice per currently-lit furnace, keyed by
    // block position and reconciled each frame by ReconcileFurnaceLoops.
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const {
            return (static_cast<size_t>(static_cast<uint32_t>(v.x)) * 73856093u) ^
                   (static_cast<size_t>(static_cast<uint32_t>(v.y)) * 19349663u) ^
                   (static_cast<size_t>(static_cast<uint32_t>(v.z)) * 83492791u);
        }
    };
    std::unordered_map<glm::ivec3, vox::VoiceHandle, IVec3Hash> m_furnaceLoops;

    // Footstep cadence + ground/water edge tracking (see UpdateLocomotion).
    double m_stepDistance = 0.0; // horizontal travel since the last footstep
    glm::vec3 m_lastFootPos{0.0f};
    bool m_footInit = false; // re-seeded by ResetLocomotion
    bool m_wasGrounded = true;
    bool m_wasInWater = false;

    double m_digSoundAccum = 0.0; // mining-tick dig sound pacing

    // Schedulers.
    double m_ambientTimer = 30.0; // seconds of darkness until the next cave sound
    double m_musicGap = 90.0;     // seconds until the first track
    uint32_t m_rng = 0x9e3779b9u;
};

} // namespace vc
