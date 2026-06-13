#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include <glm/glm.hpp>

namespace vox {

// Volume buses. Master multiplies everything; the others let the game balance
// SFX vs music vs ambience independently (and a future settings screen expose
// them). Each played sound is tagged with one of the non-Master buses.
enum class AudioBus : uint8_t { Master, Sfx, Music, Ambient };

// Opaque handle to a loaded, cached clip. Default-constructed == invalid;
// Play* on an invalid handle is a silent no-op (this is what a clean clone
// with no gitignored assets/mc/sounds/ overlay produces).
struct ClipHandle {
    uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
};

// Opaque handle to an active looping/persistent voice (furnace crackle,
// ambient loop, music). Generation-tagged so a Stop after the voice was
// auto-reaped is a safe no-op.
struct VoiceHandle {
    uint32_t id = 0;
    uint32_t gen = 0;
    explicit operator bool() const { return id != 0; }
};

// Facade over miniaudio's high-level engine (mixing graph + spatializer).
// Mirrors vox::Renderer's role for graphics: game code never touches the audio
// backend directly. Ogg Vorbis clips are decoded with stb_vorbis at load and
// played back through miniaudio data sources. All public calls are made from
// the main thread; miniaudio runs its own audio thread internally.
//
// PIMPL keeps miniaudio.h out of this header — the game links the engine but
// not miniaudio.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Opens the default device and starts the mixing engine. Returns false and
    // leaves the engine in a silent no-op state if no device is available
    // (headless/CI, no sound card) — the game keeps running mute.
    bool Init();
    // Stops every voice and tears down the engine. Idempotent; the destructor
    // calls it too.
    void Shutdown();
    bool Initialized() const;

    // --- Loading -----------------------------------------------------------
    // Decodes a short clip fully into memory and caches it (zero-latency
    // replay; re-loading the same path returns the cached handle). A missing
    // file logs once and returns an invalid handle.
    ClipHandle LoadClip(const std::filesystem::path& path);

    // --- Fire-and-forget one-shots -----------------------------------------
    // volume/pitch are multipliers (1.0 = nominal). Non-positional (UI / global).
    void Play2D(ClipHandle clip, AudioBus bus = AudioBus::Sfx, float volume = 1.0f,
                float pitch = 1.0f);
    // Positional at a world point; attenuates with listener distance.
    void Play3D(ClipHandle clip, const glm::vec3& worldPos, AudioBus bus = AudioBus::Sfx,
                float volume = 1.0f, float pitch = 1.0f);

    // --- Looping / persistent voices ---------------------------------------
    // Start a looping voice; returns a handle to stop later (invalid if the
    // clip is invalid). 3D voices attenuate with distance.
    VoiceHandle PlayLoop3D(ClipHandle clip, const glm::vec3& worldPos, AudioBus bus,
                           float volume = 1.0f, float pitch = 1.0f);
    VoiceHandle PlayLoop2D(ClipHandle clip, AudioBus bus, float volume = 1.0f);
    void SetVoicePosition(VoiceHandle voice, const glm::vec3& worldPos);
    void SetVoiceVolume(VoiceHandle voice, float volume);
    void StopVoice(VoiceHandle voice);                   // immediate
    void FadeOutVoice(VoiceHandle voice, float seconds); // then auto-stops
    bool IsVoiceActive(VoiceHandle voice) const;

    // --- Music (one streamed-style track at a time) ------------------------
    // Decodes `path` on demand, plays it once (non-positional, Music bus), and
    // frees the PCM when it ends — so only the current track sits in RAM
    // (music files are minutes long; pre-decoding them all wastes ~250 MB and
    // seconds of startup). Replaces any track already playing.
    void PlayMusic(const std::filesystem::path& path, float volume = 1.0f);
    bool MusicActive() const; // a track is currently playing

    // --- Listener (call once per frame from the camera) --------------------
    void SetListener(const glm::vec3& position, const glm::vec3& forward,
                     const glm::vec3& up);

    // --- Volume buses ------------------------------------------------------
    void SetBusVolume(AudioBus bus, float volume01);
    float BusVolume(AudioBus bus) const;

    // Per-frame housekeeping: reaps finished one-shot voices and advances
    // fades. Cheap — no mixing happens here (that's on miniaudio's thread).
    void Update(double frameDt);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vox
