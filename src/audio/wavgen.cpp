#include "audio/wavgen.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rd {

float NoiseSample(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xFFFFFF) / static_cast<float>(0x7FFFFF) - 1.0f;
}

std::vector<int16_t> SynthesizePcm16(const SynthParams& p, int sampleRate) {
    const size_t frames = static_cast<size_t>(std::max(1.0f, p.duration * static_cast<float>(sampleRate)));
    std::vector<int16_t> out(frames);
    double phase = 0.0;
    uint32_t noise = p.seed ? p.seed : 1u;
    float held = 0.f;
    const float ratio = (p.freqStart > 0.f && p.freqEnd > 0.f) ? p.freqEnd / p.freqStart : 1.f;
    for (size_t i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float u = static_cast<float>(i) / static_cast<float>(frames);
        float freq = p.freqStart * std::pow(ratio, u);
        if (p.vibratoHz > 0.f) freq *= 1.f + p.vibratoDepth * std::sin(2.f * PI * p.vibratoHz * t);
        phase += static_cast<double>(freq) / sampleRate;
        phase -= std::floor(phase);
        const float ph = static_cast<float>(phase);
        float v = 0.f;
        switch (p.osc) {
            case Osc::Sine: v = std::sin(2.f * PI * ph); break;
            case Osc::Square: v = ph < p.dutyCycle ? 1.f : -1.f; break;
            case Osc::Saw: v = 2.f * ph - 1.f; break;
            case Osc::Triangle: v = 4.f * std::fabs(ph - 0.5f) - 1.f; break;
            case Osc::Noise:
                // sample & hold noise at `freq` -> pitched noise
                if (ph < static_cast<float>(freq) / sampleRate || i == 0) held = NoiseSample(noise);
                v = held;
                break;
        }
        // AR envelope
        float env = 1.f;
        if (p.attack > 0.f && t < p.attack) env = t / p.attack;
        const float remaining = p.duration - t;
        if (p.release > 0.f && remaining < p.release) env *= std::max(0.f, remaining / p.release);
        const float s = std::clamp(v * env * p.volume, -1.f, 1.f);
        out[i] = static_cast<int16_t>(std::lround(s * 32767.f));
    }
    return out;
}

static void Put32(std::vector<unsigned char>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
static void Put16(std::vector<unsigned char>& b, uint16_t v) {
    b.push_back(static_cast<unsigned char>(v & 0xFF));
    b.push_back(static_cast<unsigned char>(v >> 8));
}

std::vector<unsigned char> EncodeWav16(const int16_t* pcm, size_t frames, int sampleRate, int channels) {
    const uint32_t dataBytes = static_cast<uint32_t>(frames * static_cast<size_t>(channels) * 2u);
    std::vector<unsigned char> b;
    b.reserve(44 + dataBytes);
    b.insert(b.end(), {'R', 'I', 'F', 'F'});
    Put32(b, 36u + dataBytes);
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    Put32(b, 16);                                                    // fmt chunk size
    Put16(b, 1);                                                     // PCM
    Put16(b, static_cast<uint16_t>(channels));
    Put32(b, static_cast<uint32_t>(sampleRate));
    Put32(b, static_cast<uint32_t>(sampleRate * channels * 2));      // byte rate
    Put16(b, static_cast<uint16_t>(channels * 2));                   // block align
    Put16(b, 16);                                                    // bits per sample
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    Put32(b, dataBytes);
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(pcm);
    b.insert(b.end(), raw, raw + dataBytes);  // host is little endian (x86/ARM)
    return b;
}

Wave MakeWave(const SynthParams& p, int sampleRate) {
    const auto pcm = SynthesizePcm16(p, sampleRate);
    const auto wav = EncodeWav16(pcm.data(), pcm.size(), sampleRate, 1);
    return LoadWaveFromMemory(".wav", wav.data(), static_cast<int>(wav.size()));
}

std::vector<int16_t> ComposeMusicLoop(int sampleRate, float bpm, int bars, uint32_t seed) {
    const float beat = 60.f / bpm;
    const float totalSec = beat * 4.f * static_cast<float>(bars);
    const size_t frames = static_cast<size_t>(totalSec * static_cast<float>(sampleRate));
    std::vector<float> mix(frames, 0.f);
    // A minor-ish progression: Am F C G (root midi notes)
    const int roots[4] = {57, 53, 60, 55};
    const int arp[4] = {0, 3, 7, 12};
    auto midiHz = [](float n) { return 440.f * std::pow(2.f, (n - 69.f) / 12.f); };
    uint32_t noise = seed ? seed : 7u;
    const int sixteenths = bars * 16;
    const size_t step = static_cast<size_t>(beat / 4.f * static_cast<float>(sampleRate));
    for (int s = 0; s < sixteenths; ++s) {
        const int bar = (s / 16) % 4;
        const size_t start = static_cast<size_t>(s) * step;
        const float bassHz = midiHz(static_cast<float>(roots[bar] - 12));
        const float arpHz = midiHz(static_cast<float>(roots[bar] + arp[s % 4] + ((s / 4) % 2 ? 12 : 0)));
        for (size_t i = 0; i < step && start + i < frames; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-t * 18.f);
            float v = 0.f;
            // triangle bass on 8ths
            if (s % 2 == 0) {
                const float ph = std::fmod(t * bassHz, 1.f);
                v += (4.f * std::fabs(ph - 0.5f) - 1.f) * 0.35f * std::exp(-t * 6.f);
            }
            // square arp
            const float pa = std::fmod(t * arpHz, 1.f);
            v += (pa < 0.25f ? 1.f : -1.f) * 0.12f * env;
            // hat on off-beats, kick-ish thump on beats
            if (s % 4 == 2) v += NoiseSample(noise) * 0.08f * std::exp(-t * 60.f);
            if (s % 4 == 0) v += std::sin(2.f * PI * (60.f + 90.f * std::exp(-t * 30.f)) * t) * 0.35f * std::exp(-t * 12.f);
            mix[start + i] += v;
        }
    }
    std::vector<int16_t> out(frames);
    for (size_t i = 0; i < frames; ++i) out[i] = static_cast<int16_t>(std::lround(std::clamp(mix[i], -1.f, 1.f) * 30000.f));
    return out;
}

float ComputeRms(const float* samples, size_t count) {
    if (!samples || count == 0) return 0.f;
    double acc = 0.0;
    for (size_t i = 0; i < count; ++i) acc += static_cast<double>(samples[i]) * samples[i];
    return static_cast<float>(std::sqrt(acc / static_cast<double>(count)));
}

float ComputePeak(const float* samples, size_t count) {
    float p = 0.f;
    for (size_t i = 0; i < count; ++i) p = std::max(p, std::fabs(samples[i]));
    return p;
}

float OnePoleLowPass::AlphaFor(float cutoffHz, float sampleRate) {
    const float rc = 1.f / (2.f * PI * cutoffHz);
    const float dt = 1.f / sampleRate;
    return dt / (rc + dt);
}

}  // namespace rd
