// audio_system.hpp - all audio is synthesized at startup (no asset files):
// Sounds (+aliases), a Music stream from memory, a callback AudioStream (engine hum),
// a push-mode AudioStream (ambient pad), stream/mixed processors (low-pass, VU meter).
#pragma once
#include "game/config.hpp"
#include "game/storage.hpp"
#include "raylib.h"

#include <vector>

namespace rd {

enum class Sfx { Shoot, Hit, Explode, Pickup, Heal, Dash, Hurt, Wave, Ui, Count };
constexpr int kSfxCount = static_cast<int>(Sfx::Count);
const char* SfxName(Sfx s);

class AudioSystem {
public:
    bool init(Storage& storage, const Settings& s);
    void shutdown();
    void update(float dt);  // UpdateMusicStream + push stream refill
    void applySettings(const Settings& s);
    bool ready() const { return ready_; }

    void play(Sfx s, float pitchJitter = 0.f, float pan = 0.5f);
    void setDanger(float d);  // 0..1 -> music pitch / pad intensity
    void setHum(bool on, float intensity = 0.5f);
    void setLowPass(bool on);
    bool lowPass() const { return lowPass_; }
    void pauseAll(bool paused);
    void fadeOutAndClose(float seconds);  // WaitTime based fade on quit

    // showcase access
    Sound& sound(Sfx s) { return sounds_[static_cast<int>(s)]; }
    Music& music() { return music_; }
    AudioStream& humStream() { return hum_; }
    AudioStream& padStream() { return pad_; }
    bool humOn() const { return humOn_; }
    bool padOn() const { return padOn_; }
    void setPad(bool on);
    const std::vector<float>& waveform(Sfx s) const { return waveforms_[static_cast<int>(s)]; }
    float vuLevel() const;          // RMS from the mixed processor
    float musicProgress() const;    // 0..1
    bool reloadMusicFromDisk();     // LoadMusicStream(file) instead of memory
    bool musicFromDisk() const { return musicFromDisk_; }

private:
    void buildSounds(Storage& storage);
    void refillPad();

    bool ready_ = false;
    Sound sounds_[kSfxCount]{};
    Sound shootAliases_[4]{};
    int shootNext_ = 0;
    std::vector<float> waveforms_[kSfxCount];
    std::vector<unsigned char> musicBytes_;  // must outlive LoadMusicStreamFromMemory
    Music music_{};
    bool musicFromDisk_ = false;
    std::string musicPath_;
    AudioStream hum_{}, pad_{};
    bool humOn_ = false, padOn_ = false, lowPass_ = false;
    std::vector<short> padBuffer_;
    float padPhase_ = 0.f, padPhase2_ = 0.f;
    float danger_ = 0.f;
    float sfxVolume_ = 0.8f, musicVolume_ = 0.5f;
    Wave hurtWave_{};
};

}  // namespace rd
