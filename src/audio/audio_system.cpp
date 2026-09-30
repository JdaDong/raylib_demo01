#include "audio/audio_system.hpp"

#include "audio/wavgen.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace rd {

const char* SfxName(Sfx s) {
    static const char* names[] = {"Shoot", "Hit", "Explode", "Pickup", "Heal", "Dash", "Hurt", "Wave", "Ui"};
    const int i = static_cast<int>(s);
    return (i >= 0 && i < kSfxCount) ? names[i] : "?";
}

namespace {
// ---- audio-thread state (callbacks are plain C function pointers) -----------
std::atomic<float> g_humFreq{55.f};
std::atomic<float> g_humAmp{0.f};
std::atomic<float> g_vu{0.f};
float g_humPhase = 0.f;
OnePoleLowPass g_lpL, g_lpR;
constexpr int kHumRate = 44100;
constexpr int kPadRate = 22050;
constexpr int kPadFrames = 4096;

// AudioStream callback: fills mono 16-bit frames on demand
void HumCallback(void* buffer, unsigned int frames) {
    short* d = static_cast<short*>(buffer);
    const float f = g_humFreq.load(), a = g_humAmp.load();
    for (unsigned int i = 0; i < frames; ++i) {
        g_humPhase += f / static_cast<float>(kHumRate);
        if (g_humPhase >= 1.f) g_humPhase -= 1.f;
        const float v = std::sin(2.f * PI * g_humPhase) * 0.7f + std::sin(4.f * PI * g_humPhase + 0.3f) * 0.3f;
        d[i] = static_cast<short>(v * a * 9000.f);
    }
}

// Stream processor (attached to the music stream): float32 stereo interleaved
void LowPassProcessor(void* buffer, unsigned int frames) {
    float* s = static_cast<float*>(buffer);
    for (unsigned int i = 0; i < frames; ++i) {
        s[i * 2] = g_lpL.process(s[i * 2]);
        s[i * 2 + 1] = g_lpR.process(s[i * 2 + 1]);
    }
}

// Mixed processor: sees the final mix of every sound/music/stream
void VuProcessor(void* buffer, unsigned int frames) {
    const float* s = static_cast<const float*>(buffer);
    const float rms = ComputeRms(s, static_cast<size_t>(frames) * 2u);
    const float prev = g_vu.load();
    g_vu.store(std::max(rms, prev * 0.85f));
}

SynthParams ParamsFor(Sfx s) {
    SynthParams p;
    switch (s) {
        case Sfx::Shoot: p = {Osc::Square, 880, 440, 0.08f, 0.22f, 0.002f, 0.05f, 0, 0, 0.25f, 1}; break;
        case Sfx::Hit: p = {Osc::Noise, 3000, 900, 0.07f, 0.3f, 0.001f, 0.05f, 0, 0, 0.5f, 2}; break;
        case Sfx::Explode: p = {Osc::Noise, 900, 50, 0.55f, 0.55f, 0.002f, 0.45f, 0, 0, 0.5f, 3}; break;
        case Sfx::Pickup: p = {Osc::Square, 660, 1320, 0.1f, 0.22f, 0.002f, 0.05f, 0, 0, 0.5f, 4}; break;
        case Sfx::Heal: p = {Osc::Sine, 440, 880, 0.35f, 0.4f, 0.01f, 0.2f, 8.f, 0.05f, 0.5f, 5}; break;
        case Sfx::Dash: p = {Osc::Saw, 180, 900, 0.16f, 0.3f, 0.005f, 0.1f, 0, 0, 0.5f, 6}; break;
        case Sfx::Hurt: p = {Osc::Saw, 320, 70, 0.28f, 0.45f, 0.002f, 0.2f, 30.f, 0.08f, 0.5f, 7}; break;
        case Sfx::Wave: p = {Osc::Triangle, 330, 660, 0.7f, 0.45f, 0.02f, 0.4f, 6.f, 0.03f, 0.5f, 8}; break;
        case Sfx::Ui: p = {Osc::Sine, 1250, 1250, 0.1f, 0.3f, 0.001f, 0.02f, 0, 0, 0.5f, 9}; break;
        default: break;
    }
    return p;
}
}  // namespace

bool AudioSystem::init(Storage& storage, const Settings& s) {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        TraceLog(LOG_WARNING, "AUDIO: device not available, running silent");
        return false;
    }
    buildSounds(storage);

    // music: synthesized WAV bytes -> LoadMusicStreamFromMemory (bytes must stay alive)
    const auto pcm = ComposeMusicLoop(44100, 128.f, 8, 7);
    musicBytes_ = EncodeWav16(pcm.data(), pcm.size(), 44100, 1);
    musicPath_ = storage.path("exports/music.wav");
    SaveFileData(musicPath_.c_str(), musicBytes_.data(), static_cast<int>(musicBytes_.size()));
    music_ = LoadMusicStreamFromMemory(".wav", musicBytes_.data(), static_cast<int>(musicBytes_.size()));
    if (IsMusicValid(music_)) {
        music_.looping = true;
        PlayMusicStream(music_);
    }

    // callback stream (pull)
    hum_ = LoadAudioStream(kHumRate, 16, 1);
    if (IsAudioStreamValid(hum_)) SetAudioStreamCallback(hum_, HumCallback);

    // push stream with an explicit buffer size
    SetAudioStreamBufferSizeDefault(kPadFrames);
    pad_ = LoadAudioStream(kPadRate, 16, 1);
    SetAudioStreamBufferSizeDefault(0);  // back to raylib's default for later streams
    padBuffer_.assign(kPadFrames, 0);

    AttachAudioMixedProcessor(VuProcessor);
    g_lpL.alpha = g_lpR.alpha = OnePoleLowPass::AlphaFor(700.f, 44100.f);
    applySettings(s);
    TraceLog(LOG_INFO, "AUDIO: ready, master volume %.2f, music %.1fs", GetMasterVolume(), GetMusicTimeLength(music_));
    return true;
}

void AudioSystem::buildSounds(Storage& storage) {
    for (int i = 0; i < kSfxCount; ++i) {
        const Sfx s = static_cast<Sfx>(i);
        Wave w = MakeWave(ParamsFor(s), 44100);
        if (!IsWaveValid(w)) continue;
        if (s == Sfx::Ui) WaveCrop(&w, 0, static_cast<int>(w.frameCount * 3 / 10));  // short click

        // downsampled float copy for waveform visualisation
        Wave viz = WaveCopy(w);
        WaveFormat(&viz, 11025, 32, 1);
        float* samples = LoadWaveSamples(viz);
        auto& out = waveforms_[i];
        const unsigned int n = viz.frameCount;
        const int points = 256;
        out.resize(points);
        for (int k = 0; k < points; ++k) out[static_cast<size_t>(k)] = samples[static_cast<size_t>(k) * n / points];
        UnloadWaveSamples(samples);
        UnloadWave(viz);

        if (s == Sfx::Explode) {
            // exercise the file based loaders: export -> LoadWave / LoadSound
            const std::string path = storage.path("exports/explode.wav");
            ExportWave(w, path.c_str());
            Wave fromDisk = LoadWave(path.c_str());
            TraceLog(LOG_INFO, "AUDIO: explode.wav reloaded, %u frames", fromDisk.frameCount);
            UnloadWave(fromDisk);
            sounds_[i] = LoadSound(path.c_str());
        } else {
            if (s == Sfx::Pickup) ExportWaveAsCode(w, storage.path("exports/pickup_wave.h").c_str());
            if (s == Sfx::Hurt) hurtWave_ = WaveCopy(w);
            sounds_[i] = LoadSoundFromWave(w);
        }
        UnloadWave(w);
    }
    for (auto& a : shootAliases_) a = LoadSoundAlias(sounds_[static_cast<int>(Sfx::Shoot)]);
}

void AudioSystem::applySettings(const Settings& s) {
    if (!ready_) return;
    SetMasterVolume(s.masterVolume);
    sfxVolume_ = s.sfxVolume;
    musicVolume_ = s.musicVolume;
    for (auto& snd : sounds_)
        if (IsSoundValid(snd)) SetSoundVolume(snd, sfxVolume_);
    for (auto& a : shootAliases_) SetSoundVolume(a, sfxVolume_ * 0.6f);
    if (IsMusicValid(music_)) SetMusicVolume(music_, musicVolume_);
    if (IsAudioStreamValid(hum_)) SetAudioStreamVolume(hum_, sfxVolume_);
    if (IsAudioStreamValid(pad_)) SetAudioStreamVolume(pad_, musicVolume_ * 0.5f);
}

void AudioSystem::play(Sfx s, float pitchJitter, float pan) {
    if (!ready_) return;
    const int i = static_cast<int>(s);
    Sound* snd = &sounds_[i];
    if (s == Sfx::Shoot) {  // round-robin aliases -> overlapping shots share one buffer
        snd = &shootAliases_[shootNext_];
        shootNext_ = (shootNext_ + 1) % 4;
    }
    if (!IsSoundValid(*snd)) return;
    if (s == Sfx::Hurt && !IsSoundPlaying(*snd) && IsWaveValid(hurtWave_)) {
        // UpdateSound: rewrite the sound buffer with a fresh variation (same frame count)
        Wave v = WaveCopy(hurtWave_);
        WaveFormat(&v, static_cast<int>(snd->stream.sampleRate), static_cast<int>(snd->stream.sampleSize),
                   static_cast<int>(snd->stream.channels));
        if (v.frameCount >= snd->frameCount) {
            // subtle random gain so repeated hits differ
            float* f = LoadWaveSamples(v);
            const float g = 0.8f + static_cast<float>(GetRandomValue(0, 40)) / 100.f;
            if (snd->stream.sampleSize == 32) {
                float* dst = static_cast<float*>(v.data);
                for (unsigned int k = 0; k < snd->frameCount * snd->stream.channels; ++k) dst[k] = f[k] * g;
            }
            UnloadWaveSamples(f);
            UpdateSound(*snd, v.data, static_cast<int>(snd->frameCount));
        }
        UnloadWave(v);
    }
    if (s == Sfx::Explode && IsSoundPlaying(*snd)) StopSound(*snd);
    const float jitter = pitchJitter * static_cast<float>(GetRandomValue(-100, 100)) / 100.f;
    SetSoundPitch(*snd, 1.f + jitter);
    SetSoundPan(*snd, std::clamp(pan, 0.f, 1.f));
    PlaySound(*snd);
}

void AudioSystem::refillPad() {
    const float base = 110.f * (1.f + danger_ * 0.5f);
    for (int i = 0; i < kPadFrames; ++i) {
        padPhase_ += base / kPadRate;
        padPhase2_ += base * 1.498f / kPadRate;  // fifth
        if (padPhase_ > 1.f) padPhase_ -= 1.f;
        if (padPhase2_ > 1.f) padPhase2_ -= 1.f;
        const float v = std::sin(2.f * PI * padPhase_) * 0.5f + std::sin(2.f * PI * padPhase2_) * 0.35f;
        padBuffer_[static_cast<size_t>(i)] = static_cast<short>(v * 4000.f * (0.4f + danger_ * 0.6f));
    }
}

void AudioSystem::update(float /*dt*/) {
    if (!ready_) return;
    if (IsMusicValid(music_) && IsMusicStreamPlaying(music_)) UpdateMusicStream(music_);
    if (padOn_ && IsAudioStreamValid(pad_)) {
        for (int guard = 0; guard < 2 && IsAudioStreamProcessed(pad_); ++guard) {
            refillPad();
            UpdateAudioStream(pad_, padBuffer_.data(), kPadFrames);
        }
    }
}

void AudioSystem::setDanger(float d) {
    danger_ = std::clamp(d, 0.f, 1.f);
    if (!ready_) return;
    if (IsMusicValid(music_)) SetMusicPitch(music_, 1.f + danger_ * 0.06f);
    g_humFreq.store(50.f + danger_ * 40.f);
}

void AudioSystem::setHum(bool on, float intensity) {
    if (!ready_ || !IsAudioStreamValid(hum_)) return;
    g_humAmp.store(std::clamp(intensity, 0.f, 1.f));
    if (on && !humOn_) PlayAudioStream(hum_);
    if (!on && humOn_) StopAudioStream(hum_);
    humOn_ = on;
}

void AudioSystem::setPad(bool on) {
    if (!ready_ || !IsAudioStreamValid(pad_)) return;
    if (on && !padOn_) {
        refillPad();
        UpdateAudioStream(pad_, padBuffer_.data(), kPadFrames);
        PlayAudioStream(pad_);
    }
    if (!on && padOn_) StopAudioStream(pad_);
    padOn_ = on;
}

void AudioSystem::setLowPass(bool on) {
    if (!ready_ || !IsMusicValid(music_) || on == lowPass_) return;
    if (on) AttachAudioStreamProcessor(music_.stream, LowPassProcessor);
    else DetachAudioStreamProcessor(music_.stream, LowPassProcessor);
    lowPass_ = on;
}

void AudioSystem::pauseAll(bool paused) {
    if (!ready_) return;
    if (IsMusicValid(music_)) {
        if (paused) PauseMusicStream(music_);
        else ResumeMusicStream(music_);
    }
    if (humOn_) {
        if (paused) PauseAudioStream(hum_);
        else ResumeAudioStream(hum_);
    }
    if (padOn_) {
        if (paused) PauseAudioStream(pad_);
        else ResumeAudioStream(pad_);
    }
}

float AudioSystem::vuLevel() const { return g_vu.load(); }

float AudioSystem::musicProgress() const {
    if (!ready_ || !IsMusicValid(music_)) return 0.f;
    const float len = GetMusicTimeLength(music_);
    return len > 0.f ? GetMusicTimePlayed(music_) / len : 0.f;
}

bool AudioSystem::reloadMusicFromDisk() {
    if (!ready_) return false;
    const bool wasLowPass = lowPass_;
    setLowPass(false);
    StopMusicStream(music_);
    UnloadMusicStream(music_);
    musicFromDisk_ = !musicFromDisk_;
    music_ = musicFromDisk_ ? LoadMusicStream(musicPath_.c_str())
                            : LoadMusicStreamFromMemory(".wav", musicBytes_.data(), static_cast<int>(musicBytes_.size()));
    if (!IsMusicValid(music_)) return false;
    music_.looping = true;
    SetMusicVolume(music_, musicVolume_);
    PlayMusicStream(music_);
    setLowPass(wasLowPass);
    return true;
}

void AudioSystem::fadeOutAndClose(float seconds) {
    if (!ready_) return;
    const float start = GetMasterVolume();
    for (int i = 1; i <= 20; ++i) {
        SetMasterVolume(start * (1.f - static_cast<float>(i) / 20.f));
        if (IsMusicValid(music_)) UpdateMusicStream(music_);
        WaitTime(seconds / 20.f);
    }
    shutdown();
}

void AudioSystem::shutdown() {
    if (!ready_) return;
    DetachAudioMixedProcessor(VuProcessor);
    setLowPass(false);
    if (IsAudioStreamValid(hum_)) UnloadAudioStream(hum_);
    if (IsAudioStreamValid(pad_)) UnloadAudioStream(pad_);
    if (IsMusicValid(music_)) {
        StopMusicStream(music_);
        UnloadMusicStream(music_);
    }
    for (auto& a : shootAliases_)
        if (IsSoundValid(a)) UnloadSoundAlias(a);  // aliases before their source
    for (auto& s : sounds_)
        if (IsSoundValid(s)) UnloadSound(s);
    if (IsWaveValid(hurtWave_)) UnloadWave(hurtWave_);
    CloseAudioDevice();
    ready_ = false;
}

}  // namespace rd
