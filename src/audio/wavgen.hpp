// wavgen.hpp - procedural audio: PCM synthesis + in-memory RIFF/WAV encoder.
// Output is fed to raylib LoadWaveFromMemory / LoadMusicStreamFromMemory.
#pragma once
#include "raylib.h"

#include <cstdint>
#include <vector>

namespace rd {

enum class Osc { Sine, Square, Saw, Triangle, Noise };

struct SynthParams {
    Osc osc = Osc::Square;
    float freqStart = 440.f;
    float freqEnd = 440.f;   // exponential slide start -> end
    float duration = 0.2f;   // seconds
    float volume = 0.5f;
    float attack = 0.005f;
    float release = 0.08f;
    float vibratoHz = 0.f;
    float vibratoDepth = 0.f;  // fraction of freq
    float dutyCycle = 0.5f;    // square only
    uint32_t seed = 1;
};

std::vector<int16_t> SynthesizePcm16(const SynthParams& p, int sampleRate);
// 16-bit PCM WAV (RIFF) byte stream
std::vector<unsigned char> EncodeWav16(const int16_t* pcm, size_t frames, int sampleRate, int channels);
// synthesize + encode + LoadWaveFromMemory (".wav")
Wave MakeWave(const SynthParams& p, int sampleRate);

// Small chiptune loop (bass + arpeggio + hat), mono, 16-bit
std::vector<int16_t> ComposeMusicLoop(int sampleRate, float bpm, int bars, uint32_t seed);

float ComputeRms(const float* samples, size_t count);
float ComputePeak(const float* samples, size_t count);

struct OnePoleLowPass {
    float alpha = 0.1f;  // 0..1, lower = darker
    float z = 0.f;
    float process(float x) { z += alpha * (x - z); return z; }
    static float AlphaFor(float cutoffHz, float sampleRate);
};

// deterministic white noise in [-1,1]
float NoiseSample(uint32_t& state);

}  // namespace rd
