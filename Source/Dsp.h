#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
#include "Params.h"

// STEINEX TRANSIT FX - DSP modules.
// Every module works on a stereo buffer in place. A module is "engaged" while its
// ON parameter is true; engaging a module starts its transition (tape stop falls,
// riser starts, last word is thrown, ...). Tail based modules keep ringing after release.

namespace steinex::fx
{
struct Ctx
{
    double sr = 44100.0;
    double bpm = 120.0;
    double ppq = 0.0;
    bool   ppqValid = false;
};

inline double beatsToSamples (double beats, const Ctx& c)
{
    return beats * 60.0 / juce::jmax (20.0, c.bpm) * c.sr;
}

//==============================================================================
struct StereoRing
{
    std::vector<float> d[2];
    int n = 0, w = 0;

    void init (int size)
    {
        n = juce::jmax (8, size);
        d[0].assign ((size_t) n, 0.f);
        d[1].assign ((size_t) n, 0.f);
        w = 0;
    }
    void clear()
    {
        std::fill (d[0].begin(), d[0].end(), 0.f);
        std::fill (d[1].begin(), d[1].end(), 0.f);
    }
    inline void push (float l, float r)
    {
        d[0][(size_t) w] = l; d[1][(size_t) w] = r;
        if (++w >= n) w = 0;
    }
    // delay = 1 returns the most recently pushed sample
    inline float read (int ch, double delay) const
    {
        double pos = (double) w - delay;
        while (pos < 0.0) pos += (double) n;
        int i0 = (int) pos;
        const float f = (float) (pos - (double) i0);
        if (i0 >= n) i0 -= n;
        int i1 = i0 + 1;
        if (i1 >= n) i1 = 0;
        return d[ch][(size_t) i0] * (1.f - f) + d[ch][(size_t) i1] * f;
    }
    inline float readInt (int ch, int delay) const
    {
        int i = w - delay;
        while (i < 0) i += n;
        return d[ch][(size_t) i];
    }
};

//==============================================================================
class ModuleBase
{
public:
    virtual ~ModuleBase() = default;

    void bind (std::atomic<float>* onPtr, std::vector<std::atomic<float>*> params)
    {
        on = onPtr;
        prm = std::move (params);
    }

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        maxBlockSize = maxBlock;
        eng.reset (sr, (double) engageSeconds());
        eng.setCurrentAndTargetValue (0.f);
        wasOn = false;
        running.store (false);
        prepareImpl (sr, maxBlock);
    }

    void run (juce::AudioBuffer<float>& b, const Ctx& c)
    {
        const bool o = isOn();
        const bool wasRunning = running.load (std::memory_order_relaxed);

        if (o && ! wasOn)  onEngage (c, wasRunning);
        if (! o && wasOn)  onRelease (c);
        wasOn = o;
        eng.setTargetValue (o ? 1.f : 0.f);

        if (! o && ! wasRunning)
        {
            eng.setCurrentAndTargetValue (0.f);
            if (alwaysRun()) feedOnly (b, c);
            return;
        }

        processImpl (b, c);
        running.store (o || eng.getCurrentValue() > 0.0005f || tailing(), std::memory_order_relaxed);
    }

    bool isOn() const        { return on != nullptr && on->load (std::memory_order_relaxed) > 0.5f; }
    bool isRunning() const   { return running.load (std::memory_order_relaxed); }

protected:
    virtual void prepareImpl (double sr, int maxBlock) = 0;
    virtual void onEngage (const Ctx&, bool /*wasRunning*/) {}
    virtual void onRelease (const Ctx&) {}
    virtual void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) = 0;
    virtual void feedOnly (const juce::AudioBuffer<float>&, const Ctx&) {}
    virtual bool alwaysRun() const { return false; }
    virtual bool tailing() const   { return false; }
    virtual float engageSeconds() const { return 0.012f; }

    float P (int i) const { return prm[(size_t) i]->load (std::memory_order_relaxed); }

    static juce::dsp::ProcessSpec spec (double sr, int block)
    {
        juce::dsp::ProcessSpec s;
        s.sampleRate = sr; s.maximumBlockSize = (juce::uint32) juce::jmax (1, block); s.numChannels = 2;
        return s;
    }

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> eng;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    bool wasOn = false;
    std::atomic<bool> running { false };

private:
    std::atomic<float>* on = nullptr;
    std::vector<std::atomic<float>*> prm;
};

//==============================================================================
// STUTTER - repeats the last slice, tempo aligned
class Stutter : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int) override
    {
        hist.init ((int) (sr * 4.6) + 8);
        sliceCap = (int) (sr * 4.4);
        slice[0].assign ((size_t) sliceCap, 0.f);
        slice[1].assign ((size_t) sliceCap, 0.f);
        len = 1024; counter = 0; rep = 0; flip = false;
    }

    static double speedMul (float v) { const int i = (int) std::lround (v); return i == 1 ? 2.0 : (i == 2 ? 4.0 : 1.0); }
    double sliceBeats() const { return stutterDivBeats ((int) std::lround (P (st::Div))) / speedMul (P (st::Speed)); }

    int computeLen (const Ctx& c) const
    {
        return juce::jlimit (32, sliceCap, (int) beatsToSamples (sliceBeats(), c));
    }

    void capture (int rot)
    {
        for (int k = 0; k < len; ++k)
        {
            const int src = (k + rot) % len;
            const int delay = len - src;
            slice[0][(size_t) k] = hist.readInt (0, delay);
            slice[1][(size_t) k] = hist.readInt (1, delay);
        }
    }

    void onEngage (const Ctx& c, bool) override
    {
        len = computeLen (c);
        rep = 0; flip = false;
        int rot = 0;
        counter = 0;
        if (c.ppqValid)
        {
            const double ph = std::fmod (c.ppq / sliceBeats(), 1.0);
            const double phase = ph < 0.0 ? ph + 1.0 : ph;
            counter = juce::jlimit (0, len - 1, (int) (phase * len));
            rot = (int) ((1.0 - phase) * len) % len;
        }
        capture (rot);
    }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) hist.push (l[i], r[i]);
    }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };

        const int newLen = computeLen (c);
        if (newLen != len)
        {
            len = newLen;
            counter = juce::jmin (counter, len - 1);
            capture (0);
        }

        const int mode = (int) std::lround (P (st::Mode));
        const int reps = juce::jmax (1, (int) std::lround (P (st::Repeats)));
        const float gate = P (st::Gate) * 0.01f;
        const float inten = P (st::Intensity) * 0.01f;
        const int fade = juce::jmax (1, (int) (P (st::Smooth) * 0.001 * sampleRate));
        const int gateLen = juce::jmax (2, (int) (gate * (float) len));

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float l0 = d[0][i], r0 = d[1][i];

            if (counter >= len)
            {
                counter = 0;
                if (++rep >= reps) { rep = 0; capture (0); }
                if (mode == 2) flip = ! flip;
            }

            const bool rev = (mode == 1) || (mode == 2 && flip);
            const int idx = rev ? len - 1 - counter : counter;

            float env = 0.f;
            if (counter < gateLen)
                env = juce::jmin (1.f, (float) counter / (float) fade, (float) (gateLen - 1 - counter) / (float) fade);

            const float a = inten * g;
            d[0][i] = l0 * (1.f - a) + slice[0][(size_t) idx] * env * a;
            d[1][i] = r0 * (1.f - a) + slice[1][(size_t) idx] * env * a;

            hist.push (l0, r0);
            ++counter;
        }
    }

private:
    StereoRing hist;
    std::vector<float> slice[2];
    int sliceCap = 0, len = 1024, counter = 0, rep = 0;
    bool flip = false;
};

//==============================================================================
// BACKVERB - reverse reverb of the last window of audio
class Backverb : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int maxBlock) override
    {
        maxW = (int) (sr * 8.0);
        ring.init (maxW + (int) (sr * 0.5));
        play[0].assign ((size_t) maxW, 0.f);
        play[1].assign ((size_t) maxW, 0.f);
        pre.init ((int) (sr * 0.25) + 8);
        scr[0].assign ((size_t) juce::jmax (maxBlock, 64), 0.f);
        scr[1].assign ((size_t) juce::jmax (maxBlock, 64), 0.f);
        reverb.setSampleRate (sr);
        reverb.reset();
        lastRoom = -1.f;
        W = 1024; pos = 0; playing = false;
    }

    int computeW (const Ctx& c) const
    {
        double w = (P (bv::Sync) > 0.5f) ? beatsToSamples (timeBeats ((int) std::lround (P (bv::Time))), c)
                                         : (double) P (bv::Decay) * sampleRate;
        return juce::jlimit (512, maxW, (int) w);
    }

    void updateReverb()
    {
        const float room = 0.35f + 0.6f * P (bv::Size) * 0.01f;
        if (std::abs (room - lastRoom) < 1.0e-4f) return;
        lastRoom = room;
        juce::Reverb::Parameters rp;
        rp.roomSize = room; rp.damping = 0.45f; rp.wetLevel = 0.4f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
    }

    void renderWet (const float* inL, const float* inR, int n)
    {
        if ((int) scr[0].size() < n) { scr[0].resize ((size_t) n); scr[1].resize ((size_t) n); }
        const int pd = (int) (P (bv::PreDelay) * 0.001 * sampleRate);
        for (int i = 0; i < n; ++i)
        {
            pre.push (inL[i], inR[i]);
            scr[0][(size_t) i] = pre.readInt (0, pd + 1 > pre.n - 1 ? pre.n - 1 : pd + 1);
            scr[1][(size_t) i] = pre.readInt (1, pd + 1 > pre.n - 1 ? pre.n - 1 : pd + 1);
        }
        updateReverb();
        reverb.processStereo (scr[0].data(), scr[1].data(), n);
        for (int i = 0; i < n; ++i) ring.push (scr[0][(size_t) i], scr[1][(size_t) i]);
    }

    void buildPlay (const Ctx& c)
    {
        W = computeW (c);
        const int fi = juce::jmax (1, (int) (0.004 * sampleRate));
        const int fo = juce::jmax (1, (int) (0.012 * sampleRate));
        for (int k = 0; k < W; ++k)
        {
            float e = 1.f;
            if (k < fi)         e = (float) k / (float) fi;
            if (k > W - 1 - fo) e = juce::jmin (e, (float) (W - 1 - k) / (float) fo);
            play[0][(size_t) k] = ring.readInt (0, 1 + k) * e;
            play[1][(size_t) k] = ring.readInt (1, 1 + k) * e;
        }
        pos = 0;
        playing = true;
    }

    void onEngage (const Ctx& c, bool) override { buildPlay (c); }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        renderWet (b.getReadPointer (0), b.getReadPointer (1), b.getNumSamples());
    }

    bool tailing() const override { return playing; }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };
        renderWet (d[0], d[1], n);

        const float m = P (bv::Mix) * 0.01f;
        const float tail = P (bv::Tail) * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            if (playing && pos >= W)
            {
                if (isOn()) buildPlay (c);
                else { playing = false; pos = 0; }
            }
            float wl = 0.f, wr = 0.f;
            if (playing)
            {
                wl = play[0][(size_t) pos]; wr = play[1][(size_t) pos];
                ++pos;
            }
            d[0][i] = d[0][i] * (1.f - m * g) + wl * m * tail;
            d[1][i] = d[1][i] * (1.f - m * g) + wr * m * tail;
        }
    }

private:
    juce::Reverb reverb;
    StereoRing ring, pre;
    std::vector<float> play[2], scr[2];
    int maxW = 1024, W = 1024, pos = 0;
    bool playing = false;
    float lastRoom = -1.f;
};

//==============================================================================
// DELAY THROW - tempo delay with filtered feedback and a last-word throw mode
class DelayThrow : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int maxBlock) override
    {
        dline.init ((int) (sr * 8.6));
        hist.init ((int) (sr * 8.6));
        for (int c = 0; c < 2; ++c)
        {
            hp[c].prepare (spec (sr, maxBlock)); hp[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
            lp[c].prepare (spec (sr, maxBlock)); lp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }
        wetEnv = 0.f; throwRemain = throwLen = 0;
    }

    void onEngage (const Ctx& c, bool wasRunning) override
    {
        if (! wasRunning)
        {
            dline.clear();
            for (int ch = 0; ch < 2; ++ch) { hp[ch].reset(); lp[ch].reset(); }
            wetEnv = 0.f;
        }
        throwLen = juce::jlimit (64, hist.n - 8, (int) beatsToSamples (timeBeats ((int) std::lround (P (dl::Send))), c));
        throwRemain = throwLen;
    }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) hist.push (l[i], r[i]);
    }

    bool tailing() const override { return wetEnv > 1.0e-3f; }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };

        const double dly = juce::jlimit (32.0, (double) (dline.n - 8),
                                         beatsToSamples (timeBeats ((int) std::lround (P (dl::Time))), c));
        const float fb = juce::jlimit (0.f, 0.95f, P (dl::Feedback) * 0.01f);
        const float width = P (dl::Width) * 0.01f;
        const float m = P (dl::Mix) * 0.01f;
        const bool throwMode = P (dl::Mode) > 0.5f;
        const int fade = juce::jmax (1, (int) (0.002 * sampleRate));

        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].setCutoffFrequency (juce::jlimit (20.f, 8000.f, P (dl::Hpf)));
            lp[ch].setCutoffFrequency (juce::jlimit (200.f, 20000.f, P (dl::Lpf)));
        }

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float inL = d[0][i], inR = d[1][i];
            hist.push (inL, inR);

            float sL = 0.f, sR = 0.f;
            if (throwMode)
            {
                if (throwRemain > 0)
                {
                    const int k = throwLen - throwRemain;
                    const float e = juce::jmin (1.f, (float) k / (float) fade, (float) throwRemain / (float) fade);
                    sL = hist.read (0, (double) throwLen) * e;
                    sR = hist.read (1, (double) throwLen) * e;
                    --throwRemain;
                }
            }
            else { sL = inL * g; sR = inR * g; }

            const float dL = dline.read (0, dly), dR = dline.read (1, dly);
            const float fL = (dL + (dR - dL) * width) * fb;
            const float fR = (dR + (dL - dR) * width) * fb;

            float wL = lp[0].processSample (0, hp[0].processSample (0, sL + fL));
            float wR = lp[1].processSample (1, hp[1].processSample (1, sR + fR));
            wL = std::tanh (wL); wR = std::tanh (wR);
            dline.push (wL, wR);

            const float e = juce::jmax (std::abs (dL), std::abs (dR));
            wetEnv = juce::jmax (e, wetEnv * 0.9997f);

            d[0][i] = inL * (1.f - 0.35f * m) + dL * m;
            d[1][i] = inR * (1.f - 0.35f * m) + dR * m;
        }
    }

private:
    StereoRing dline, hist;
    juce::dsp::StateVariableTPTFilter<float> hp[2], lp[2];
    float wetEnv = 0.f;
    int throwRemain = 0, throwLen = 0;
};

//==============================================================================
// TAPE STOP - variable speed playback from a history buffer
class TapeStop : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int) override { hist.init ((int) (sr * 8.6)); lag = 1.0; t = 0.0; }

    void onEngage (const Ctx&, bool) override { lag = 1.0; t = 0.0; }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) hist.push (l[i], r[i]);
    }

    static double shape (int curve, double u)
    {
        switch (curve)
        {
            case 1:  return u * u;
            case 2:  return 1.0 - (1.0 - u) * (1.0 - u);
            case 3:  return u * u * (3.0 - 2.0 * u);
            default: return u;
        }
    }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };
        const double dur = juce::jmax (0.05, (double) P (tp::Time)) * sampleRate;
        const double depth = P (tp::Depth) * 0.01;
        const int curve = (int) std::lround (P (tp::Curve));

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float l0 = d[0][i], r0 = d[1][i];
            hist.push (l0, r0);

            const double u = juce::jmin (1.0, t / dur);
            const double rate = 1.0 - depth * shape (curve, u);
            float tl = 0.f, tr = 0.f;
            if (lag < (double) (hist.n - 8))
            {
                const float tg = juce::jmin (1.f, (float) rate * 8.f);
                tl = hist.read (0, lag) * tg;
                tr = hist.read (1, lag) * tg;
                lag += 1.0 - rate;
            }
            t += 1.0;

            d[0][i] = l0 * (1.f - g) + tl * g;
            d[1][i] = r0 * (1.f - g) + tr * g;
        }
    }

private:
    StereoRing hist;
    double lag = 1.0, t = 0.0;
};

//==============================================================================
// PITCH RISE / DROP - dual tap crossfaded delay line pitch shifter with a ramped ratio
class PitchRamp : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int) override { ring.init ((int) (sr * 0.45)); phase = 0.0; cur = 0.0; }

    void onEngage (const Ctx&, bool) override { cur = 0.0; }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) ring.push (l[i], r[i]);
    }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };

        const double Wp = juce::jlimit (64.0, (double) (ring.n - 8), (double) P (ps::Window) * 0.001 * sampleRate);
        const double range = P (ps::Range);
        const double target = (P (ps::Mode) > 0.5f ? -1.0 : 1.0) * range;
        const double step = range / juce::jmax (1.0, (double) P (ps::Speed) * sampleRate);
        const float mix = P (ps::Mix) * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float l0 = d[0][i], r0 = d[1][i];
            ring.push (l0, r0);

            if (cur < target)      cur = juce::jmin (target, cur + step);
            else if (cur > target) cur = juce::jmax (target, cur - step);

            const double ratio = std::exp2 (cur / 12.0);
            phase += (1.0 - ratio) / Wp;
            phase -= std::floor (phase);
            double p2 = phase + 0.5;
            if (p2 >= 1.0) p2 -= 1.0;

            const float s1 = (float) std::sin (juce::MathConstants<double>::pi * phase);
            const float s2 = (float) std::sin (juce::MathConstants<double>::pi * p2);
            const float g1 = s1 * s1, g2 = s2 * s2;
            const double d1 = 1.0 + phase * Wp, d2 = 1.0 + p2 * Wp;

            const float wl = ring.read (0, d1) * g1 + ring.read (0, d2) * g2;
            const float wr = ring.read (1, d1) * g1 + ring.read (1, d2) * g2;
            const float a = mix * g;
            d[0][i] = l0 * (1.f - a) + wl * a;
            d[1][i] = r0 * (1.f - a) + wr * a;
        }
    }

private:
    StereoRing ring;
    double phase = 0.0, cur = 0.0;
};

//==============================================================================
// GRANULAR GLITCH
class GranularGlitch : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    struct Grain { bool active = false; double delay = 0, inc = 0; int age = 0, len = 1; };

    void prepareImpl (double sr, int) override
    {
        hist.init ((int) (sr * 2.4));
        for (auto& g : grains) g.active = false;
        lastStep = -1; phaseSteps = 0.0;
    }

    void onEngage (const Ctx&, bool) override { for (auto& g : grains) g.active = false; }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) hist.push (l[i], r[i]);
    }

    bool stepHit (int step, float inten)
    {
        static const int masks[] { 0xFFFF, 0xAAAA, 0x4949, 0xBBBB, 0xFFFF };
        const int pat = juce::jlimit (0, 4, (int) std::lround (P (gl::Pattern)));
        bool hit = (masks[pat] >> (step & 15)) & 1;
        if (pat == 4) hit = rng.nextFloat() < 0.55f;
        return hit && rng.nextFloat() < (0.25f + 0.75f * inten);
    }

    void spawn()
    {
        Grain* gr = nullptr;
        for (auto& g : grains) if (! g.active) { gr = &g; break; }
        if (gr == nullptr) return;

        const int len = juce::jmax (64, (int) (P (gl::Grain) * 0.001 * sampleRate));
        const float rnd = P (gl::Random) * 0.01f;
        const bool rev = rng.nextFloat() < P (gl::Reverse) * 0.01f;
        const double ratio = std::exp2 ((double) ((rng.nextFloat() * 2.f - 1.f) * rnd));
        const double back = (double) rng.nextFloat() * rnd * 0.5 * sampleRate;

        gr->len = len; gr->age = 0; gr->active = true;
        if (rev) { gr->delay = 2.0 + back;                        gr->inc = 1.0 + ratio; }
        else     { gr->delay = (double) len * ratio + 2.0 + back; gr->inc = 1.0 - ratio; }
    }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };

        const double stepBeats = timeBeats ((int) std::lround (P (gl::Rate)));
        const double stepLen = juce::jmax (16.0, beatsToSamples (stepBeats, c));
        if (c.ppqValid) phaseSteps = c.ppq / stepBeats;
        const float inten = P (gl::Intensity) * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float l0 = d[0][i], r0 = d[1][i];
            hist.push (l0, r0);

            phaseSteps += 1.0 / stepLen;
            const int stepIdx = (int) std::floor (phaseSteps);
            if (stepIdx != lastStep)
            {
                lastStep = stepIdx;
                if (g > 0.05f && stepHit (stepIdx, inten)) spawn();
            }

            float gl0 = 0.f, gl1 = 0.f, envSum = 0.f;
            for (auto& gr : grains)
            {
                if (! gr.active) continue;
                const float w = 0.5f * (1.f - std::cos (juce::MathConstants<float>::twoPi * (float) gr.age / (float) gr.len));
                if (gr.delay < 1.0 || gr.delay > (double) (hist.n - 4)) { gr.active = false; continue; }
                gl0 += hist.read (0, gr.delay) * w;
                gl1 += hist.read (1, gr.delay) * w;
                envSum += w;
                gr.delay += gr.inc;
                if (++gr.age >= gr.len) gr.active = false;
            }

            const float duck = inten * g * juce::jmin (1.f, envSum);
            const float amp = g * (0.7f + 0.3f * inten);
            d[0][i] = l0 * (1.f - duck) + gl0 * amp;
            d[1][i] = r0 * (1.f - duck) + gl1 * amp;
        }
    }

private:
    StereoRing hist;
    Grain grains[6];
    juce::Random rng { 0x57e11e5 };
    double phaseSteps = 0.0;
    int lastStep = -1;
};

//==============================================================================
// REVERB FREEZE
class ReverbFreeze : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int maxBlock) override
    {
        reverb.setSampleRate (sr);
        reverb.reset();
        scr[0].assign ((size_t) juce::jmax (maxBlock, 64), 0.f);
        scr[1].assign ((size_t) juce::jmax (maxBlock, 64), 0.f);
        frozen = false; wg = 0.f; lastRoom = lastDamp = -1.f; lastFrozen = false;
    }

    void updateReverb()
    {
        const float room = 0.5f + 0.49f * P (fz::Size) * 0.01f;
        const float damp = P (fz::Damp) * 0.01f;
        if (room == lastRoom && damp == lastDamp && frozen == lastFrozen) return;
        lastRoom = room; lastDamp = damp; lastFrozen = frozen;
        juce::Reverb::Parameters rp;
        rp.roomSize = room; rp.damping = damp; rp.wetLevel = 0.4f; rp.dryLevel = 0.f; rp.width = 1.f;
        rp.freezeMode = frozen ? 1.f : 0.f;
        reverb.setParameters (rp);
    }

    void onEngage (const Ctx&, bool) override
    {
        frozen = true;
        holdLeft = (int) ((double) P (fz::Hold) * sampleRate);
    }
    void onRelease (const Ctx&) override { frozen = false; }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const int n = b.getNumSamples();
        if ((int) scr[0].size() < n) { scr[0].resize ((size_t) n); scr[1].resize ((size_t) n); }
        std::copy (b.getReadPointer (0), b.getReadPointer (0) + n, scr[0].begin());
        std::copy (b.getReadPointer (1), b.getReadPointer (1) + n, scr[1].begin());
        updateReverb();
        reverb.processStereo (scr[0].data(), scr[1].data(), n);
    }

    bool tailing() const override { return wg > 0.002f; }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };
        if ((int) scr[0].size() < n) { scr[0].resize ((size_t) n); scr[1].resize ((size_t) n); }
        std::copy (d[0], d[0] + n, scr[0].begin());
        std::copy (d[1], d[1] + n, scr[1].begin());
        updateReverb();
        reverb.processStereo (scr[0].data(), scr[1].data(), n);

        const bool timed = P (fz::Mode) > 0.5f;
        const float m = P (fz::Mix) * 0.01f;
        const float relCoef = (float) std::exp (-6.9078 / ((double) juce::jmax (0.1f, P (fz::Release)) * sampleRate));
        const float atk = 1.f - (float) std::exp (-1.0 / (0.008 * sampleRate));

        for (int i = 0; i < n; ++i)
        {
            if (frozen && timed && --holdLeft <= 0) { frozen = false; updateReverb(); }
            if (frozen) wg += (1.f - wg) * atk; else wg *= relCoef;
            d[0][i] = d[0][i] * (1.f - m * wg) + scr[0][(size_t) i] * m * wg;
            d[1][i] = d[1][i] * (1.f - m * wg) + scr[1][(size_t) i] * m * wg;
        }
    }

private:
    juce::Reverb reverb;
    std::vector<float> scr[2];
    bool frozen = false, lastFrozen = false;
    float wg = 0.f, lastRoom = -1.f, lastDamp = -1.f;
    int holdLeft = 0;
};

//==============================================================================
// RISER / DOWNLIFTER - synthesised noise, tone and filter sweeps added to the signal
class Riser : public ModuleBase
{
protected:
    void prepareImpl (double sr, int maxBlock) override
    {
        for (int c = 0; c < 2; ++c)
        {
            lp[c].prepare (spec (sr, maxBlock)); lp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            hp[c].prepare (spec (sr, maxBlock)); hp[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
            hp[c].setCutoffFrequency (180.f);
        }
        t = 0.0; tonePhase = 0.0;
    }

    void onEngage (const Ctx&, bool) override
    {
        t = 0.0; tonePhase = 0.0;
        for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); }
    }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };

        const bool down = P (rs::Mode) > 0.5f;
        const double L = juce::jmax (0.1, riserSeconds ((int) std::lround (P (rs::Len)), c.bpm)) * sampleRate;
        const float noiseAmt = P (rs::Noise) * 0.01f, toneAmt = P (rs::Tone) * 0.01f;
        const float depth = P (rs::Filter) * 0.01f, inten = P (rs::Intensity) * 0.01f;
        const double fLow = 220.0;
        const double fHigh = juce::jmin (16000.0, fLow * std::pow (2.0, 1.0 + 5.8 * (double) depth));
        const int edge = juce::jmax (1, (int) (0.01 * sampleRate));

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const double u = juce::jmin (1.0, t / L);
            const double cv = down ? 1.0 - u : u;

            if ((i & 15) == 0)
            {
                const float cut = (float) (fLow * std::pow (fHigh / fLow, cv));
                lp[0].setCutoffFrequency (cut); lp[1].setCutoffFrequency (cut);
            }

            float level = down ? (float) std::pow (1.0 - u, 1.4) : (float) std::pow (u, 1.8);
            if (t < (double) edge)                 level *= (float) (t / (double) edge);
            if (! down && t > L - (double) edge)   level *= (float) juce::jmax (0.0, (L - t) / (double) edge);
            if (t >= L) level = 0.f;

            const double ft = 120.0 * std::pow (16.0, cv);
            tonePhase += juce::MathConstants<double>::twoPi * ft / sampleRate;
            if (tonePhase > juce::MathConstants<double>::twoPi) tonePhase -= juce::MathConstants<double>::twoPi;
            const float tone = (float) std::sin (tonePhase) * 0.5f;

            for (int ch = 0; ch < 2; ++ch)
            {
                const float noise = rng.nextFloat() * 2.f - 1.f;
                const float nf = lp[ch].processSample (ch, hp[ch].processSample (ch, noise));
                const float synth = (nf * noiseAmt * 0.9f + tone * toneAmt * 0.5f) * level * inten * 0.9f;
                d[ch][i] += synth * g;
            }
            t += 1.0;
        }
    }

private:
    juce::dsp::StateVariableTPTFilter<float> lp[2], hp[2];
    juce::Random rng { 0x1234abc };
    double t = 0.0, tonePhase = 0.0;
};

//==============================================================================
// REVERSE DELAY - chunked reverse echo with feedback
class ReverseDelay : public ModuleBase
{
public:
    bool alwaysRun() const override { return true; }

protected:
    void prepareImpl (double sr, int maxBlock) override
    {
        maxW = (int) (sr * 8.0);
        hist.init (maxW + 64);
        for (int c = 0; c < 2; ++c)
        {
            rec[c].assign ((size_t) maxW, 0.f);
            ply[c].assign ((size_t) maxW, 0.f);
            hp[c].prepare (spec (sr, maxBlock)); hp[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
            lp[c].prepare (spec (sr, maxBlock)); lp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }
        Wrec = Wply = 1024; pos = 0; wetEnv = 0.f;
    }

    int computeW (const Ctx& c) const
    {
        return juce::jlimit (256, maxW, (int) beatsToSamples (timeBeats ((int) std::lround (P (rd::Time))), c));
    }

    void onEngage (const Ctx& c, bool wasRunning) override
    {
        if (! wasRunning)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                std::fill (rec[ch].begin(), rec[ch].end(), 0.f);
                hp[ch].reset(); lp[ch].reset();
            }
            wetEnv = 0.f;
        }
        Wrec = computeW (c);
        Wply = Wrec;
        for (int k = 0; k < Wply; ++k)
        {
            ply[0][(size_t) k] = hist.readInt (0, Wply - k);
            ply[1][(size_t) k] = hist.readInt (1, Wply - k);
        }
        pos = 0;
    }

    void feedOnly (const juce::AudioBuffer<float>& b, const Ctx&) override
    {
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i) hist.push (l[i], r[i]);
    }

    bool tailing() const override { return wetEnv > 1.0e-3f; }

    void processImpl (juce::AudioBuffer<float>& b, const Ctx& c) override
    {
        const int n = b.getNumSamples();
        float* d[2] { b.getWritePointer (0), b.getWritePointer (1) };
        const float fb = juce::jlimit (0.f, 0.9f, P (rd::Feedback) * 0.01f);
        const float stereo = P (rd::Stereo) * 0.01f;
        const float m = P (rd::Mix) * 0.01f;
        const int fade = juce::jmax (1, (int) (0.004 * sampleRate));

        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].setCutoffFrequency (juce::jlimit (20.f, 8000.f, P (rd::Hpf)));
            lp[ch].setCutoffFrequency (juce::jlimit (200.f, 20000.f, P (rd::Lpf)));
        }

        for (int i = 0; i < n; ++i)
        {
            const float g = eng.getNextValue();
            const float inL = d[0][i], inR = d[1][i];
            hist.push (inL, inR);

            if (pos >= Wrec)
            {
                std::swap (rec[0], ply[0]); std::swap (rec[1], ply[1]);
                Wply = Wrec;
                Wrec = computeW (c);
                pos = 0;
            }

            float wL = 0.f, wR = 0.f;
            if (pos < Wply)
            {
                const int pi = Wply - 1 - pos;
                const float e = juce::jmin (1.f, (float) pos / (float) fade, (float) pi / (float) fade);
                wL = ply[0][(size_t) pi] * e;
                wR = ply[1][(size_t) pi] * e;
            }

            const float fL = (wL + (wR - wL) * stereo) * fb;
            const float fR = (wR + (wL - wR) * stereo) * fb;
            float xL = lp[0].processSample (0, hp[0].processSample (0, inL * g + fL));
            float xR = lp[1].processSample (1, hp[1].processSample (1, inR * g + fR));
            rec[0][(size_t) pos] = std::tanh (xL);
            rec[1][(size_t) pos] = std::tanh (xR);
            ++pos;

            wetEnv = juce::jmax (juce::jmax (std::abs (wL), std::abs (wR)), wetEnv * 0.9997f);
            d[0][i] = inL * (1.f - 0.35f * m) + wL * m;
            d[1][i] = inR * (1.f - 0.35f * m) + wR * m;
        }
    }

private:
    StereoRing hist;
    std::vector<float> rec[2], ply[2];
    juce::dsp::StateVariableTPTFilter<float> hp[2], lp[2];
    int maxW = 1024, Wrec = 1024, Wply = 1024, pos = 0;
    float wetEnv = 0.f;
};

//==============================================================================
inline std::unique_ptr<ModuleBase> createModule (int id)
{
    switch (id)
    {
        case kStutter:  return std::make_unique<Stutter>();
        case kBackverb: return std::make_unique<Backverb>();
        case kDelay:    return std::make_unique<DelayThrow>();
        case kTape:     return std::make_unique<TapeStop>();
        case kPitch:    return std::make_unique<PitchRamp>();
        case kGlitch:   return std::make_unique<GranularGlitch>();
        case kFreeze:   return std::make_unique<ReverbFreeze>();
        case kRiser:    return std::make_unique<Riser>();
        case kRevDelay: return std::make_unique<ReverseDelay>();
        default:        return nullptr;
    }
}
} // namespace steinex::fx
