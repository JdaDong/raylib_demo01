// wavgen: PCM synthesis, RIFF encoding and raylib Wave APIs (no audio device)
#include "audio/wavgen.hpp"
#include "testing.hpp"

#include <algorithm>
#include <cstring>
#include <string>

using namespace rd;

TEST(wavgen, synth_length_and_range) {
    SynthParams p;
    p.duration = 0.25f;
    p.volume = 1.0f;
    const auto pcm = SynthesizePcm16(p, 22050);
    CHECK_EQ(pcm.size(), static_cast<size_t>(0.25f * 22050));
    const auto [mn, mx] = std::minmax_element(pcm.begin(), pcm.end());
    CHECK(*mx <= 32767 && *mn >= -32767);
    CHECK(*mx > 20000);  // loud enough
}

TEST(wavgen, envelope_starts_and_ends_silent) {
    SynthParams p;
    p.osc = Osc::Sine;
    p.duration = 0.2f;
    p.attack = 0.02f;
    p.release = 0.05f;
    const auto pcm = SynthesizePcm16(p, 44100);
    CHECK(std::abs(pcm.front()) < 200);
    CHECK(std::abs(pcm.back()) < 200);
}

TEST(wavgen, all_oscillators_are_deterministic) {
    for (Osc o : {Osc::Sine, Osc::Square, Osc::Saw, Osc::Triangle, Osc::Noise}) {
        SynthParams p;
        p.osc = o;
        p.seed = 77;
        CHECK(SynthesizePcm16(p, 8000) == SynthesizePcm16(p, 8000));
    }
}

TEST(wavgen, riff_header_layout) {
    const int16_t pcm[4] = {0, 1000, -1000, 32767};
    const auto wav = EncodeWav16(pcm, 4, 22050, 1);
    REQUIRE(wav.size() == 44u + 8u);
    CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0);
    CHECK(std::memcmp(wav.data() + 8, "WAVEfmt ", 8) == 0);
    CHECK(std::memcmp(wav.data() + 36, "data", 4) == 0);
    const uint32_t riffSize = wav[4] | (wav[5] << 8) | (wav[6] << 16) | (static_cast<uint32_t>(wav[7]) << 24);
    CHECK_EQ(riffSize, 36u + 8u);
    const uint32_t rate = wav[24] | (wav[25] << 8) | (wav[26] << 16);
    CHECK_EQ(rate, 22050u);
}

TEST(wavgen, raylib_loads_wave_from_memory) {
    SynthParams p;
    p.duration = 0.1f;
    Wave w = MakeWave(p, 22050);
    REQUIRE(IsWaveValid(w));
    CHECK_EQ(w.sampleRate, 22050u);
    CHECK_EQ(w.sampleSize, 16u);
    CHECK_EQ(w.channels, 1u);
    CHECK_EQ(w.frameCount, static_cast<unsigned int>(0.1f * 22050));
    UnloadWave(w);
}

TEST(wavgen, wave_copy_crop_format_samples) {
    SynthParams p;
    p.duration = 0.2f;
    Wave w = MakeWave(p, 22050);
    Wave c = WaveCopy(w);
    CHECK_EQ(c.frameCount, w.frameCount);
    WaveCrop(&c, 100, 1100);
    CHECK_EQ(c.frameCount, 1000u);
    WaveFormat(&c, 44100, 32, 2);  // resample + float + stereo
    CHECK_EQ(c.sampleRate, 44100u);
    CHECK_EQ(c.sampleSize, 32u);
    CHECK_EQ(c.channels, 2u);
    CHECK(c.frameCount >= 1990u && c.frameCount <= 2010u);
    float* s = LoadWaveSamples(c);
    REQUIRE(s != nullptr);
    const size_t n = static_cast<size_t>(c.frameCount) * c.channels;
    CHECK(ComputePeak(s, n) <= 1.0001f);
    CHECK(ComputeRms(s, n) > 0.01f);
    UnloadWaveSamples(s);
    UnloadWave(c);
    UnloadWave(w);
}

TEST(wavgen, export_and_reload) {
    SynthParams p;
    p.duration = 0.05f;
    Wave w = MakeWave(p, 22050);
    const std::string path = std::string(RD_TEST_TMP) + "/sfx_test.wav";
    CHECK(ExportWave(w, path.c_str()));
    Wave back = LoadWave(path.c_str());
    CHECK(IsWaveValid(back));
    CHECK_EQ(back.frameCount, w.frameCount);
    const std::string code = std::string(RD_TEST_TMP) + "/sfx_test.h";
    CHECK(ExportWaveAsCode(w, code.c_str()));
    CHECK(GetFileLength(code.c_str()) > 100);
    UnloadWave(back);
    UnloadWave(w);
}

TEST(wavgen, music_loop_length) {
    const auto m = ComposeMusicLoop(22050, 120.f, 2, 3);
    // 2 bars of 4/4 at 120 bpm = 4 s
    CHECK(m.size() >= static_cast<size_t>(4.0f * 22050) - 10);
    CHECK(m.size() <= static_cast<size_t>(4.0f * 22050) + 10);
    int16_t peak = 0;
    for (int16_t s : m) peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(s)));
    CHECK(peak > 3000);
}

TEST(wavgen, rms_and_peak) {
    const float sq[4] = {1, -1, 1, -1};
    CHECK_NEAR(ComputeRms(sq, 4), 1.0, 1e-6);
    CHECK_NEAR(ComputePeak(sq, 4), 1.0, 1e-6);
    CHECK_NEAR(ComputeRms(nullptr, 0), 0.0, 1e-9);
}

TEST(wavgen, low_pass_attenuates_high_frequencies) {
    OnePoleLowPass lp;
    lp.alpha = OnePoleLowPass::AlphaFor(500.f, 44100.f);
    CHECK(lp.alpha > 0.f && lp.alpha < 1.f);
    float outAmp = 0.f;
    for (int i = 0; i < 4410; ++i) {
        const float x = (i % 2) ? 1.f : -1.f;  // Nyquist square
        const float y = lp.process(x);
        if (i > 1000) outAmp = std::max(outAmp, std::fabs(y));
    }
    CHECK(outAmp < 0.1f);
    OnePoleLowPass dc;
    dc.alpha = 0.1f;
    float y = 0;
    for (int i = 0; i < 500; ++i) y = dc.process(1.f);
    CHECK_NEAR(y, 1.0, 1e-3);  // DC passes
}

TEST(wavgen, noise_in_range) {
    uint32_t s = 1;
    for (int i = 0; i < 10000; ++i) {
        const float n = NoiseSample(s);
        CHECK(n >= -1.001f && n <= 1.001f);
    }
}
