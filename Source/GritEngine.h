#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

//==============================================================================
// Small self-contained biquad (RBJ cookbook). No allocations, safe on audio thread.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    void setHighPass (double sr, double freq, double q) noexcept
    {
        const auto [cs, alpha] = trig (sr, freq, q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }

    void setLowPass (double sr, double freq, double q) noexcept
    {
        const auto [cs, alpha] = trig (sr, freq, q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }

    void setPeak (double sr, double freq, double q, double gainDb) noexcept
    {
        const auto [cs, alpha] = trig (sr, freq, q);
        const double A = std::pow (10.0, gainDb / 40.0);
        set (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
    }

private:
    static std::pair<double, double> trig (double sr, double freq, double q) noexcept
    {
        freq = std::clamp (freq, 10.0, sr * 0.45);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        return { std::cos (w0), std::sin (w0) / (2.0 * q) };
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = (float) (nb0 * inv);
        b1 = (float) (nb1 * inv);
        b2 = (float) (nb2 * inv);
        a1 = (float) (na1 * inv);
        a2 = (float) (na2 * inv);
    }
};

//==============================================================================
struct GritParams
{
    float driveDb = 25.0f;   // pre-clipping gain, 0..50 dB
    float tightHz = 120.0f;  // high-pass in front of the clipper (keeps low strings from going flubby)
    float tone    = 0.5f;    // 0..1 -> post-clip low-pass, dark to bright
    float gateDb  = -80.0f;  // noise gate threshold; <= -79 means gate off
    float levelDb = 0.0f;    // output level
    float mix     = 1.0f;    // dry/wet
    int   mode    = 1;       // 0 Crunch, 1 Lead, 2 Fuzz
    bool  cab     = true;    // simple guitar-cab style roll-off
};

//==============================================================================
class GritEngine
{
public:
    static constexpr int maxChannels = 2;

    GritEngine()
        : oversampling ((size_t) maxChannels, 2,   // 2^2 = 4x oversampling
                        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                        true, true)                // max quality, integer latency
    {}

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        chunkSize = std::clamp (maxBlockSize, 1, maxChunk);
        oversampling.initProcessing ((size_t) chunkSize);
        oversampling.reset();

        latency = (int) std::lround (oversampling.getLatencyInSamples());

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) chunkSize, 1 };
        for (auto& d : dryDelay)
        {
            d.prepare (spec);
            d.setMaximumDelayInSamples (latency + 8);
            d.setDelay ((float) latency);
        }

        driveSmooth.reset (sampleRate * 4.0, 0.03);
        levelSmooth.reset (sampleRate, 0.03);
        mixSmooth.reset (sampleRate, 0.03);
        driveSmooth.setCurrentAndTargetValue (1.0f);
        levelSmooth.setCurrentAndTargetValue (1.0f);
        mixSmooth.setCurrentAndTargetValue (1.0f);

        gateEnv.fill (0.0f);
        gateGain.fill (1.0f);
        gateAttack  = 1.0f - std::exp (-1.0f / (0.001f * (float) sr));
        gateRelease = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
        envRelease  = std::exp (-1.0f / (0.020f * (float) sr));

        dcCoeff = 1.0f - (2.0f * juce::MathConstants<float>::pi * 20.0f / (float) sr);

        lastSnapshot = {};
        haveSnapshot = false;
        reset();
    }

    void reset()
    {
        oversampling.reset();
        for (auto& d : dryDelay) d.reset();
        for (auto& c : chan) c.reset();
        gateEnv.fill (0.0f);
        gateGain.fill (1.0f);
    }

    int getLatencySamples() const noexcept { return latency; }

    // Handles any host block size by feeding the engine in chunks it was prepared for.
    void process (juce::AudioBuffer<float>& buffer, const GritParams& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), maxChannels);
        const int total = buffer.getNumSamples();

        for (int start = 0; start < total; start += chunkSize)
        {
            const int n = std::min (chunkSize, total - start);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), numCh, start, n);
            processChunk (sub, p);
        }
    }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, const GritParams& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), maxChannels);
        const int numSamples = buffer.getNumSamples();
        if (numCh == 0 || numSamples == 0)
            return;

        updateCoefficients (p);

        // Crunch tames the gain a little, Fuzz pushes it a lot.
        const float modeGainDb = p.mode == 0 ? -6.0f : (p.mode == 2 ? 10.0f : 0.0f);
        driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.driveDb + modeGainDb));
        levelSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.levelDb));
        mixSmooth.setTargetValue (p.mix);

        const bool gateOn = p.gateDb > -79.0f;
        const float gateThresh = juce::Decibels::decibelsToGain (p.gateDb);

        // ---- Stage 1: gate + tighten + mid push (at normal sample rate) ----------
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            auto& c = chan[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float in = x[i];

                // Dry path, delayed to line up with the oversampled wet path.
                dryDelay[(size_t) ch].pushSample (0, in);
                dryBuf[(size_t) ch][(size_t) i] = dryDelay[(size_t) ch].popSample (0);

                float s = in;

                if (gateOn)
                {
                    const float a = std::abs (s);
                    float& env = gateEnv[(size_t) ch];
                    env = a > env ? a : env * envRelease + a * (1.0f - envRelease);

                    const float target = env > gateThresh ? 1.0f : 0.0f;
                    float& g = gateGain[(size_t) ch];
                    g += (target - g) * (target > g ? gateAttack : gateRelease);
                    s *= g;
                }

                s = c.tightHP1.process (s);
                s = c.tightHP2.process (s);   // two stages -> 24 dB/oct, properly tight
                s = c.midPush.process (s);

                x[i] = s;
            }
        }

        // ---- Stage 2: the actual distortion, 4x oversampled ----------------------
        {
            juce::dsp::AudioBlock<float> block (buffer);
            auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);
            auto up = oversampling.processSamplesUp (sub);

            const size_t upSamples = up.getNumSamples();
            for (size_t i = 0; i < upSamples; ++i)
            {
                const float g = driveSmooth.getNextValue();
                for (size_t ch = 0; ch < (size_t) numCh; ++ch)
                {
                    auto* u = up.getChannelPointer (ch);
                    u[i] = shape (u[i] * g, p.mode);
                }
            }

            oversampling.processSamplesDown (sub);
        }

        // ---- Stage 3: clean-up, tone, cab, level, mix ----------------------------
        for (int i = 0; i < numSamples; ++i)
        {
            const float lvl = levelSmooth.getNextValue() * outputComp;
            const float mx = mixSmooth.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& c = chan[(size_t) ch];
                float s = buffer.getReadPointer (ch)[i];

                // DC blocker (asymmetric clipping creates DC)
                const float dcOut = s - c.dcX + dcCoeff * c.dcY;
                c.dcX = s;
                c.dcY = dcOut;
                s = dcOut;

                s = c.toneLP.process (s);

                if (p.cab)
                {
                    s = c.cabHP.process (s);
                    s = c.cabLP1.process (s);
                    s = c.cabLP2.process (s);
                }

                s *= lvl;

                const float dry = dryBuf[(size_t) ch][(size_t) i];
                buffer.getWritePointer (ch)[i] = dry * (1.0f - mx) + s * mx;
            }
        }
    }

public:
    // Exposed for tests
    static float shape (float x, int mode) noexcept
    {
        switch (mode)
        {
            case 0: // Crunch: soft, symmetric-ish with a touch of bias for even harmonics
            {
                constexpr float bias = 0.08f;
                return std::tanh (x + bias) - std::tanh (bias);
            }
            case 2: // Fuzz: very hard, biased, gated-sounding at low levels
            {
                constexpr float bias = 0.25f;
                return std::tanh (4.0f * (x + bias)) - std::tanh (4.0f * bias);
            }
            default: // Lead: asymmetric, diode-ish; positive side clips harder than negative
            {
                constexpr float k = 0.85f;
                return x >= 0.0f ? 1.0f - std::exp (-x)
                                 : -k * (1.0f - std::exp (x / k));
            }
        }
    }

private:
    struct Channel
    {
        Biquad tightHP1, tightHP2, midPush, toneLP, cabHP, cabLP1, cabLP2;
        float dcX = 0.0f, dcY = 0.0f;

        void reset()
        {
            tightHP1.reset(); tightHP2.reset(); midPush.reset();
            toneLP.reset(); cabHP.reset(); cabLP1.reset(); cabLP2.reset();
            dcX = dcY = 0.0f;
        }
    };

    struct Snapshot
    {
        float tightHz = -1.0f, tone = -1.0f;
        int mode = -1;
    };

    void updateCoefficients (const GritParams& p)
    {
        const bool changed = ! haveSnapshot
                             || p.tightHz != lastSnapshot.tightHz
                             || p.tone != lastSnapshot.tone
                             || p.mode != lastSnapshot.mode;
        if (! changed)
            return;

        haveSnapshot = true;
        lastSnapshot = { p.tightHz, p.tone, p.mode };

        const double toneHz = 1500.0 * std::pow (14000.0 / 1500.0, (double) p.tone);
        const double midDb = p.mode == 0 ? 3.0 : (p.mode == 1 ? 6.0 : 0.0);

        for (auto& c : chan)
        {
            c.tightHP1.setHighPass (sr, p.tightHz, 0.5412);
            c.tightHP2.setHighPass (sr, p.tightHz, 1.3066);
            c.midPush.setPeak (sr, 750.0, 0.7, midDb);
            c.toneLP.setLowPass (sr, toneHz, 0.707);
            c.cabHP.setHighPass (sr, 80.0, 0.707);
            c.cabLP1.setLowPass (sr, 5200.0, 0.5412);
            c.cabLP2.setLowPass (sr, 5200.0, 1.3066);
        }
    }

    juce::dsp::Oversampling<float> oversampling;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>, maxChannels> dryDelay;

    std::array<Channel, maxChannels> chan;
    std::array<float, maxChannels> gateEnv {}, gateGain {};
    static constexpr int maxChunk = 8192;
    std::array<std::array<float, maxChunk>, maxChannels> dryBuf {};
    int chunkSize = maxChunk;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveSmooth;
    juce::SmoothedValue<float> levelSmooth, mixSmooth;

    Snapshot lastSnapshot;
    bool haveSnapshot = false;

    double sr = 44100.0;
    int latency = 0;
    float gateAttack = 0.01f, gateRelease = 0.001f, envRelease = 0.99f, dcCoeff = 0.999f;
    static constexpr float outputComp = 0.45f;
};
