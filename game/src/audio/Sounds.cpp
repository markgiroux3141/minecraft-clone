#include "audio/Sounds.h"

#include "vox/core/Assets.h"
#include "vox/core/Log.h"

namespace vc {

namespace {

// SoundType -> the assets/mc/sounds/{dig,step}/<material>N.ogg folder. Glass
// has no dig/step assets (only a break shatter) so it borrows stone; None has
// no clips at all.
const char* MaterialFolder(SoundType type) {
    switch (type) {
    case SoundType::Stone: return "stone";
    case SoundType::Wood: return "wood";
    case SoundType::Grass: return "grass";
    case SoundType::Gravel: return "gravel";
    case SoundType::Sand: return "sand";
    case SoundType::Snow: return "snow";
    case SoundType::Cloth: return "cloth";
    case SoundType::Glass: return "stone"; // dig/step fall back to stone
    case SoundType::None: return nullptr;
    }
    return nullptr;
}

} // namespace

GameSounds::ClipSet GameSounds::LoadSet(const char* folder, const char* material,
                                        int maxVariants) {
    ClipSet set;
    if (material == nullptr) return set;
    for (int i = 1; i <= maxVariants && set.count < kMaxVariants; ++i) {
        const std::string rel =
            std::string("mc/sounds/") + folder + "/" + material + std::to_string(i) + ".ogg";
        vox::ClipHandle h = m_audio->LoadClip(vox::assets::Resolve(rel));
        if (h) set.clips[set.count++] = h;
    }
    return set;
}

void GameSounds::Load(vox::AudioEngine& audio) {
    m_audio = &audio;
    if (!audio.Initialized()) return;

    // dig has 1..4 variants per material, step has up to 6 — LoadSet keeps only
    // the variants that actually exist, so a missing one is silently skipped.
    for (int t = 0; t < kSoundTypeCount; ++t) {
        const char* mat = MaterialFolder(static_cast<SoundType>(t));
        m_dig[t] = LoadSet("dig", mat, 6);
        m_step[t] = LoadSet("step", mat, 6);
    }
    m_glassBreak = LoadSet("random", "glass", 3);
    // Splash files are "splash.ogg" (no 1) and "splash2.ogg", so the
    // material+N probe doesn't cover the first one — load both by name.
    for (const char* name : {"liquid/splash.ogg", "liquid/splash2.ogg"}) {
        const std::string rel = std::string("mc/sounds/") + name;
        if (vox::ClipHandle h = audio.LoadClip(vox::assets::Resolve(rel));
            h && m_splash.count < kMaxVariants) {
            m_splash.clips[m_splash.count++] = h;
        }
    }
    // M26 buckets: item/bucket/{fill,fill_lava,empty,empty_lava}{1..3}.
    m_bucketFill = LoadSet("item/bucket", "fill", 3);
    m_bucketFillLava = LoadSet("item/bucket", "fill_lava", 3);
    m_bucketEmpty = LoadSet("item/bucket", "empty", 3);
    m_bucketEmptyLava = LoadSet("item/bucket", "empty_lava", 3);
    m_caveAmbient = LoadSet("ambient/cave", "cave", 18); // 1.12 has 18
    m_hurt = LoadSet("damage", "hit", 3);                // entity.player.hurt

    // M32 mob voices. The numbered "say"/"hurt" variants go through LoadSet;
    // the single unnumbered "death.ogg" is loaded by name (LoadSet probes
    // <name>N.ogg). Anything missing is silently skipped.
    m_pigSay = LoadSet("mob/pig", "say", 3);
    m_zombieSay = LoadSet("mob/zombie", "say", 3);
    m_zombieHurt = LoadSet("mob/zombie", "hurt", 3);
    for (auto* dst : {&m_pigDeath, &m_zombieDeath}) {
        const char* folder = (dst == &m_pigDeath) ? "pig" : "zombie";
        const std::string rel = std::string("mc/sounds/mob/") + folder + "/death.ogg";
        if (vox::ClipHandle h = audio.LoadClip(vox::assets::Resolve(rel)); h) {
            dst->clips[dst->count++] = h;
        }
    }

    m_pop = audio.LoadClip(vox::assets::Resolve("mc/sounds/random/pop.ogg"));
    m_fireLoop = audio.LoadClip(vox::assets::Resolve("mc/sounds/fire/fire.ogg"));

    // Music: a handful of tracks played one at a time with long gaps. We keep
    // resolved PATHS and decode on demand (AudioEngine::PlayMusic) so only the
    // current track is ever in RAM — keep only ones that actually exist.
    for (const char* name :
         {"calm1", "calm2", "calm3", "hal1", "hal2", "hal3", "hal4", "piano1", "piano2",
          "piano3", "nuance1", "nuance2"}) {
        const std::filesystem::path p =
            vox::assets::Resolve(std::string("mc/sounds/music/") + name + ".ogg");
        if (std::filesystem::exists(p)) m_musicPaths.push_back(p);
    }
    GAME_INFO("audio: loaded sound sets (music tracks: {})", m_musicPaths.size());
}

float GameSounds::Jitter(float base, float spread) {
    // base * (1 +/- spread), uniform.
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    float r = static_cast<float>(m_rng & 0xFFFFFF) / static_cast<float>(0x1000000); // 0..1
    return base * (1.0f - spread + 2.0f * spread * r);
}

vox::ClipHandle GameSounds::Pick(const ClipSet& set) {
    if (set.count == 0) return {};
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return set.clips[m_rng % static_cast<uint32_t>(set.count)];
}

const GameSounds::ClipSet& GameSounds::DigSet(SoundType type) const {
    return m_dig[static_cast<int>(type)];
}
const GameSounds::ClipSet& GameSounds::StepSet(SoundType type) const {
    return m_step[static_cast<int>(type)];
}

void GameSounds::PlayDig(SoundType type, const glm::vec3& blockCenter) {
    if (!m_audio) return;
    // Vanilla mining tick: quieter and pitched down vs the break.
    m_audio->Play3D(Pick(DigSet(type)), blockCenter, vox::AudioBus::Sfx, 0.23f,
                    Jitter(0.5f, 0.05f));
}

void GameSounds::PlayBreak(SoundType type, const glm::vec3& blockCenter) {
    if (!m_audio) return;
    vox::ClipHandle clip = (type == SoundType::Glass) ? Pick(m_glassBreak) : Pick(DigSet(type));
    m_audio->Play3D(clip, blockCenter, vox::AudioBus::Sfx, 0.8f, Jitter(0.85f, 0.05f));
}

void GameSounds::PlayPlace(SoundType type, const glm::vec3& blockCenter) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(DigSet(type)), blockCenter, vox::AudioBus::Sfx, 0.8f,
                    Jitter(0.8f, 0.05f));
}

void GameSounds::PlayPickup() {
    if (!m_audio) return;
    // Vanilla pop: high, randomly pitched.
    m_audio->Play2D(m_pop, vox::AudioBus::Sfx, 0.18f, Jitter(2.0f, 0.12f));
}

void GameSounds::PlayBucketFill(bool lava, const glm::vec3& pos) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(lava ? m_bucketFillLava : m_bucketFill), pos, vox::AudioBus::Sfx, 0.7f,
                    Jitter(1.0f, 0.05f));
}

void GameSounds::PlayBucketEmpty(bool lava, const glm::vec3& pos) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(lava ? m_bucketEmptyLava : m_bucketEmpty), pos, vox::AudioBus::Sfx, 0.7f,
                    Jitter(1.0f, 0.05f));
}

void GameSounds::PlayStep(SoundType type, const glm::vec3& feet) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(StepSet(type)), feet, vox::AudioBus::Sfx, 0.28f, Jitter(1.0f, 0.05f));
}

void GameSounds::PlayLand(SoundType type, const glm::vec3& feet) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(StepSet(type)), feet, vox::AudioBus::Sfx, 0.5f, Jitter(0.9f, 0.05f));
}

void GameSounds::PlaySplash(const glm::vec3& pos) {
    if (!m_audio) return;
    m_audio->Play3D(Pick(m_splash), pos, vox::AudioBus::Sfx, 0.4f, Jitter(1.0f, 0.1f));
}

void GameSounds::PlayHurt() {
    if (!m_audio) return;
    // Vanilla: volume 1.0, pitch 1.0 +/- ~0.1. 2D — it's the player's own sound.
    m_audio->Play2D(Pick(m_hurt), vox::AudioBus::Sfx, 0.7f, Jitter(1.0f, 0.1f));
}

void GameSounds::PlayMobHurt(bool hostile, const glm::vec3& pos) {
    if (!m_audio) return;
    // Pig reuses its "say" set for hurt (vanilla has no pig hurt clip).
    const ClipSet& set = hostile ? (m_zombieHurt.count ? m_zombieHurt : m_zombieSay) : m_pigSay;
    m_audio->Play3D(Pick(set), pos, vox::AudioBus::Sfx, 0.7f, Jitter(1.0f, 0.1f));
}

void GameSounds::PlayMobDeath(bool hostile, const glm::vec3& pos) {
    if (!m_audio) return;
    const ClipSet& death = hostile ? m_zombieDeath : m_pigDeath;
    const ClipSet& say = hostile ? m_zombieSay : m_pigSay;
    m_audio->Play3D(Pick(death.count ? death : say), pos, vox::AudioBus::Sfx, 0.8f,
                    Jitter(1.0f, 0.1f));
}

vox::VoiceHandle GameSounds::StartFurnaceLoop(const glm::vec3& blockCenter) {
    if (!m_audio) return {};
    return m_audio->PlayLoop3D(m_fireLoop, blockCenter, vox::AudioBus::Sfx, 0.4f,
                               Jitter(1.0f, 0.05f));
}

void GameSounds::StopFurnaceLoop(vox::VoiceHandle& voice) {
    if (m_audio) m_audio->StopVoice(voice);
    voice = {};
}

void GameSounds::UpdateAmbient(bool inDarkness, double frameDt, const glm::vec3& listenerPos) {
    if (!m_audio || m_caveAmbient.count == 0) return;
    if (!inDarkness) {
        m_ambientTimer = 30.0; // recharge while out of the dark
        return;
    }
    m_ambientTimer -= frameDt;
    if (m_ambientTimer <= 0.0) {
        // Vanilla plays cave sounds at a random offset near the player.
        glm::vec3 off{Jitter(0.0f, 8.0f), Jitter(0.0f, 8.0f), Jitter(0.0f, 8.0f)};
        m_audio->Play3D(Pick(m_caveAmbient), listenerPos + off, vox::AudioBus::Ambient, 0.7f,
                        1.0f);
        m_ambientTimer = 20.0 + 40.0 * Jitter(0.5f, 0.5f); // ~20..60 s
    }
}

void GameSounds::UpdateMusic(double frameDt) {
    if (!m_audio || m_musicPaths.empty()) return;
    m_musicGap -= frameDt;
    if (m_musicGap <= 0.0 && !m_audio->MusicActive()) {
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        m_audio->PlayMusic(m_musicPaths[m_rng % m_musicPaths.size()], 1.0f);
        // Vanilla gaps are huge (10-20 min); this is start-to-start spacing.
        m_musicGap = 360.0 + 360.0 * Jitter(0.5f, 0.5f); // ~6..12 min
    }
}

} // namespace vc
