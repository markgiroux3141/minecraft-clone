#include "vox/audio/AudioEngine.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "miniaudio.h"

#include "vox/core/Log.h"

// stb_vorbis is compiled as its own C TU in the miniaudio static lib; we only
// need its two whole-file decode entry points here. Returns interleaved s16
// PCM in a malloc'd buffer (free with free()); return value is frames per
// channel, or -1 on failure.
extern "C" int stb_vorbis_decode_filename(const char* filename, int* channels,
                                          int* sample_rate, short** output);

namespace vox {

namespace {
constexpr float kMin3DDistance = 4.0f;  // full volume within this radius
constexpr float kMax3DDistance = 48.0f; // inaudible past here (linear rolloff)

// Decode a whole .ogg to interleaved s16 PCM via stb_vorbis. Returns false on
// failure (missing file / not vorbis).
bool DecodeOgg(const std::string& path, std::vector<int16_t>& out, ma_uint32& channels,
               ma_uint32& sampleRate, ma_uint64& frames) {
    int ch = 0, sr = 0;
    short* pcm = nullptr;
    const int f = stb_vorbis_decode_filename(path.c_str(), &ch, &sr, &pcm);
    if (f < 0 || pcm == nullptr) return false;
    out.assign(pcm, pcm + static_cast<size_t>(f) * ch);
    free(pcm);
    channels = static_cast<ma_uint32>(ch);
    sampleRate = static_cast<ma_uint32>(sr);
    frames = static_cast<ma_uint64>(f);
    return true;
}
} // namespace

// A decoded, cached clip. The PCM is owned here; voices reference it through
// their own ma_audio_buffer_ref (each ref carries an independent read cursor,
// so the same clip can play concurrently without fighting over one cursor).
struct Clip {
    std::vector<int16_t> pcm;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    ma_uint64 frameCount = 0;
};

// One active playback. ref + sound live at a stable address (slots are held by
// unique_ptr) because miniaudio stores internal pointers into both.
struct Voice {
    ma_audio_buffer_ref ref{};
    ma_sound sound{};
    bool active = false;
    bool persistent = false; // looping voice (lives until StopVoice/fade) vs one-shot
    uint32_t gen = 0;
    float fadeRemaining = -1.0f; // <0 = no fade pending
};

struct AudioEngine::Impl {
    bool initialized = false;
    ma_engine engine{};
    ma_sound_group groups[3]{}; // Sfx, Music, Ambient (index = bus-1)
    float busVolume[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    std::unordered_map<std::string, ClipHandle> clipByPath;
    std::vector<Clip> clips; // index = ClipHandle.id - 1

    std::vector<std::unique_ptr<Voice>> voices; // index = VoiceHandle.id - 1
    std::vector<uint32_t> freeVoices;
    uint32_t voiceGen = 1;

    // The single on-demand music track (decoded lazily, freed when it ends).
    std::vector<int16_t> musicPcm;
    ma_audio_buffer_ref musicRef{};
    ma_sound musicSound{};
    bool musicActive = false;

    void StopMusic() {
        if (!musicActive) return;
        ma_sound_uninit(&musicSound);
        ma_audio_buffer_ref_uninit(&musicRef);
        musicPcm.clear();
        musicPcm.shrink_to_fit();
        musicActive = false;
    }

    ma_sound_group* GroupFor(AudioBus bus) {
        if (bus == AudioBus::Master) return nullptr; // engine master, no group
        return &groups[static_cast<int>(bus) - 1];
    }

    Voice* Resolve(VoiceHandle h) {
        if (h.id == 0 || h.id > voices.size()) return nullptr;
        Voice* v = voices[h.id - 1].get();
        if (!v || !v->active || v->gen != h.gen) return nullptr;
        return v;
    }

    void ReapVoice(uint32_t index) {
        Voice* v = voices[index].get();
        ma_sound_uninit(&v->sound);
        ma_audio_buffer_ref_uninit(&v->ref);
        v->active = false;
        freeVoices.push_back(index);
    }

    // Allocates/initializes a voice playing `clip`. Returns nullptr on failure.
    Voice* StartVoice(const Clip& clip, AudioBus bus, bool spatial,
                      const glm::vec3& pos, float volume, float pitch, bool looping,
                      bool persistent, VoiceHandle* outHandle) {
        uint32_t index;
        if (!freeVoices.empty()) {
            index = freeVoices.back();
            freeVoices.pop_back();
        } else {
            voices.push_back(std::make_unique<Voice>());
            index = static_cast<uint32_t>(voices.size() - 1);
        }
        Voice* v = voices[index].get();
        *v = Voice{};

        if (ma_audio_buffer_ref_init(ma_format_s16, clip.channels, clip.pcm.data(),
                                     clip.frameCount, &v->ref) != MA_SUCCESS) {
            freeVoices.push_back(index);
            return nullptr;
        }
        v->ref.sampleRate = clip.sampleRate;

        ma_uint32 flags = 0;
        if (!spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (ma_sound_init_from_data_source(&engine, &v->ref, flags, GroupFor(bus),
                                           &v->sound) != MA_SUCCESS) {
            ma_audio_buffer_ref_uninit(&v->ref);
            freeVoices.push_back(index);
            return nullptr;
        }

        ma_sound_set_volume(&v->sound, volume);
        ma_sound_set_pitch(&v->sound, pitch);
        ma_sound_set_looping(&v->sound, looping ? MA_TRUE : MA_FALSE);
        if (spatial) {
            ma_sound_set_attenuation_model(&v->sound, ma_attenuation_model_linear);
            ma_sound_set_min_distance(&v->sound, kMin3DDistance);
            ma_sound_set_max_distance(&v->sound, kMax3DDistance);
            ma_sound_set_position(&v->sound, pos.x, pos.y, pos.z);
        }
        ma_sound_start(&v->sound);

        v->active = true;
        v->persistent = persistent;
        v->gen = voiceGen++;
        if (outHandle) *outHandle = VoiceHandle{index + 1, v->gen};
        return v;
    }
};

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() { Shutdown(); }

bool AudioEngine::Init() {
    if (m_impl->initialized) return true;

    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS) {
        VOX_WARN("audio: no device — running mute");
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->groups[i]);
    }
    m_impl->initialized = true;
    VOX_INFO("audio: device opened ({} Hz)", ma_engine_get_sample_rate(&m_impl->engine));
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_impl->initialized) return;
    m_impl->StopMusic();
    for (uint32_t i = 0; i < m_impl->voices.size(); ++i) {
        if (m_impl->voices[i] && m_impl->voices[i]->active) m_impl->ReapVoice(i);
    }
    for (int i = 0; i < 3; ++i) ma_sound_group_uninit(&m_impl->groups[i]);
    ma_engine_uninit(&m_impl->engine);
    m_impl->initialized = false;
    VOX_INFO("audio shutting down");
}

bool AudioEngine::Initialized() const { return m_impl->initialized; }

ClipHandle AudioEngine::LoadClip(const std::filesystem::path& path) {
    if (!m_impl->initialized) return {};

    const std::string key = path.string();
    if (auto it = m_impl->clipByPath.find(key); it != m_impl->clipByPath.end()) {
        return it->second;
    }

    Clip clip;
    if (!DecodeOgg(key, clip.pcm, clip.channels, clip.sampleRate, clip.frameCount)) {
        // Missing files are expected — callers probe optional variants
        // (stoneN up to 6 when only 4 exist) and a clean clone has no
        // assets/mc/sounds/ overlay at all. Cache the miss; stay silent.
        m_impl->clipByPath[key] = {};
        return {};
    }

    m_impl->clips.push_back(std::move(clip));
    ClipHandle handle{static_cast<uint32_t>(m_impl->clips.size())};
    m_impl->clipByPath[key] = handle;
    return handle;
}

void AudioEngine::Play2D(ClipHandle clip, AudioBus bus, float volume, float pitch) {
    if (!m_impl->initialized || !clip || clip.id > m_impl->clips.size()) return;
    m_impl->StartVoice(m_impl->clips[clip.id - 1], bus, /*spatial*/ false, {}, volume,
                       pitch, /*looping*/ false, /*persistent*/ false, nullptr);
}

void AudioEngine::Play3D(ClipHandle clip, const glm::vec3& worldPos, AudioBus bus,
                         float volume, float pitch) {
    if (!m_impl->initialized || !clip || clip.id > m_impl->clips.size()) return;
    m_impl->StartVoice(m_impl->clips[clip.id - 1], bus, /*spatial*/ true, worldPos, volume,
                       pitch, /*looping*/ false, /*persistent*/ false, nullptr);
}

VoiceHandle AudioEngine::PlayLoop3D(ClipHandle clip, const glm::vec3& worldPos, AudioBus bus,
                                    float volume, float pitch) {
    if (!m_impl->initialized || !clip || clip.id > m_impl->clips.size()) return {};
    VoiceHandle h;
    m_impl->StartVoice(m_impl->clips[clip.id - 1], bus, /*spatial*/ true, worldPos, volume,
                       pitch, /*looping*/ true, /*persistent*/ true, &h);
    return h;
}

VoiceHandle AudioEngine::PlayLoop2D(ClipHandle clip, AudioBus bus, float volume) {
    if (!m_impl->initialized || !clip || clip.id > m_impl->clips.size()) return {};
    VoiceHandle h;
    m_impl->StartVoice(m_impl->clips[clip.id - 1], bus, /*spatial*/ false, {}, volume, 1.0f,
                       /*looping*/ true, /*persistent*/ true, &h);
    return h;
}

void AudioEngine::SetVoicePosition(VoiceHandle voice, const glm::vec3& worldPos) {
    if (Voice* v = m_impl->Resolve(voice)) {
        ma_sound_set_position(&v->sound, worldPos.x, worldPos.y, worldPos.z);
    }
}

void AudioEngine::SetVoiceVolume(VoiceHandle voice, float volume) {
    if (Voice* v = m_impl->Resolve(voice)) ma_sound_set_volume(&v->sound, volume);
}

void AudioEngine::StopVoice(VoiceHandle voice) {
    if (voice.id == 0 || voice.id > m_impl->voices.size()) return;
    Voice* v = m_impl->voices[voice.id - 1].get();
    if (v && v->active && v->gen == voice.gen) m_impl->ReapVoice(voice.id - 1);
}

void AudioEngine::FadeOutVoice(VoiceHandle voice, float seconds) {
    if (Voice* v = m_impl->Resolve(voice)) {
        ma_sound_set_fade_in_milliseconds(&v->sound, -1.0f, 0.0f,
                                          static_cast<ma_uint64>(seconds * 1000.0f));
        v->fadeRemaining = seconds;
        v->persistent = false; // let Update reap it once the fade elapses
    }
}

bool AudioEngine::IsVoiceActive(VoiceHandle voice) const {
    return m_impl->Resolve(voice) != nullptr;
}

void AudioEngine::PlayMusic(const std::filesystem::path& path, float volume) {
    if (!m_impl->initialized) return;
    m_impl->StopMusic(); // replace any current track

    ma_uint32 ch, sr;
    ma_uint64 fr;
    if (!DecodeOgg(path.string(), m_impl->musicPcm, ch, sr, fr)) {
        m_impl->musicPcm.clear();
        return;
    }
    if (ma_audio_buffer_ref_init(ma_format_s16, ch, m_impl->musicPcm.data(), fr,
                                 &m_impl->musicRef) != MA_SUCCESS) {
        m_impl->musicPcm.clear();
        return;
    }
    m_impl->musicRef.sampleRate = sr;
    if (ma_sound_init_from_data_source(&m_impl->engine, &m_impl->musicRef,
                                       MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       m_impl->GroupFor(AudioBus::Music),
                                       &m_impl->musicSound) != MA_SUCCESS) {
        ma_audio_buffer_ref_uninit(&m_impl->musicRef);
        m_impl->musicPcm.clear();
        return;
    }
    ma_sound_set_volume(&m_impl->musicSound, volume);
    ma_sound_start(&m_impl->musicSound);
    m_impl->musicActive = true;
}

bool AudioEngine::MusicActive() const {
    return m_impl->initialized && m_impl->musicActive;
}

void AudioEngine::SetListener(const glm::vec3& position, const glm::vec3& forward,
                              const glm::vec3& up) {
    if (!m_impl->initialized) return;
    ma_engine_listener_set_position(&m_impl->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
}

void AudioEngine::SetBusVolume(AudioBus bus, float volume01) {
    if (!m_impl->initialized) return;
    m_impl->busVolume[static_cast<int>(bus)] = volume01;
    if (bus == AudioBus::Master) {
        ma_engine_set_volume(&m_impl->engine, volume01);
    } else {
        ma_sound_group_set_volume(m_impl->GroupFor(bus), volume01);
    }
}

float AudioEngine::BusVolume(AudioBus bus) const {
    return m_impl->busVolume[static_cast<int>(bus)];
}

void AudioEngine::Update(double frameDt) {
    if (!m_impl->initialized) return;
    if (m_impl->musicActive && ma_sound_at_end(&m_impl->musicSound) == MA_TRUE) {
        m_impl->StopMusic();
    }
    for (uint32_t i = 0; i < m_impl->voices.size(); ++i) {
        Voice* v = m_impl->voices[i].get();
        if (!v || !v->active) continue;
        if (v->fadeRemaining >= 0.0f) {
            v->fadeRemaining -= static_cast<float>(frameDt);
            if (v->fadeRemaining <= 0.0f) {
                m_impl->ReapVoice(i);
                continue;
            }
        }
        // One-shots self-reap when they reach the end; looping/persistent
        // voices live until an explicit Stop or a completed fade.
        if (!v->persistent && v->fadeRemaining < 0.0f &&
            ma_sound_at_end(&v->sound) == MA_TRUE) {
            m_impl->ReapVoice(i);
        }
    }
}

} // namespace vox
