// STEINEX TRANSIT FX self-test: DSP sanity, recorder, scenes and a UI snapshot.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace steinex;

static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { ++failures; std::printf("  FAIL: %s\n", msg); } } while (0)

static void setP (TransitFxProcessor& p, const juce::String& id, float v)
{
    if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v));
}

struct Gen
{
    double sr; juce::Random rng { 42 }; long t = 0;
    void fill (juce::AudioBuffer<float>& b)
    {
        for (int i = 0; i < b.getNumSamples(); ++i, ++t)
        {
            const double sec = (double) t / sr;
            const double beat = std::fmod (sec * 2.0, 1.0);                    // 120 bpm kick-ish pulse
            const float env = (float) std::exp (-beat * 6.0);
            const float s = 0.5f * env * (float) std::sin (2.0 * 3.14159265 * (180.0 + 400.0 * env) * sec)
                          + 0.2f * (float) std::sin (2.0 * 3.14159265 * 330.0 * sec) + 0.05f * (rng.nextFloat() - 0.5f);
            b.setSample (0, i, s); b.setSample (1, i, s * 0.9f);
        }
    }
};

struct Stats { bool bad = false; float peak = 0; double diff = 0; double rms = 0; long n = 0; };

static void accumulate (const juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>& in, Stats& s)
{
    for (int i = 0; i < out.getNumSamples(); ++i)
        for (int c = 0; c < 2; ++c)
        {
            const float v = out.getSample (c, i);
            if (! std::isfinite (v)) s.bad = true;
            s.peak = juce::jmax (s.peak, std::abs (v));
            s.diff += std::abs (v - in.getSample (c, i));
            s.rms += v * v; ++s.n;
        }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0; const int bs = 512;

    std::printf("== Module test (engage 1.0s..3.0s, total 10s) ==\n");
    for (int m = 0; m < kNumModules; ++m)
    {
        TransitFxProcessor p;
        p.prepareToPlay (sr, bs);
        Gen gen { sr };
        juce::MidiBuffer mb;
        juce::AudioBuffer<float> in (2, bs), buf (2, bs);
        Stats during, before; bool runningAfter = true;
        const int blocks = (int) (10.0 * sr / bs);
        for (int k = 0; k < blocks; ++k)
        {
            const double sec = (double) k * bs / sr;
            if (sec >= 1.0 && sec < 1.0 + (double) bs / sr) setP (p, onId (m), 1.f);
            if (sec >= 3.0 && sec < 3.0 + (double) bs / sr) setP (p, onId (m), 0.f);
            gen.fill (in); buf.makeCopyOf (in);
            p.processBlock (buf, mb);
            if (sec < 1.0) accumulate (buf, in, before);
            else if (sec < 3.0) accumulate (buf, in, during);
            else { Stats tmp; accumulate (buf, in, tmp); if (tmp.bad) during.bad = true; during.peak = juce::jmax (during.peak, tmp.peak); }
            runningAfter = p.isModuleRunning (m);
        }
        std::printf("  %-14s peak %.2f  changed %.3f  idle-diff %.5f  running@end %d\n",
                    moduleSpecs()[(size_t) m].name.toRawUTF8(), during.peak, during.diff / (double) juce::jmax (1L, during.n),
                    before.diff / (double) juce::jmax (1L, before.n), (int) runningAfter);
        CHECK (! during.bad, "NaN/Inf in output");
        CHECK (during.peak < 6.f, "output too loud");
        CHECK (before.diff / (double) juce::jmax (1L, before.n) < 1.0e-6, "module altered audio while off");
        CHECK (during.diff / (double) juce::jmax (1L, during.n) > 0.002, "module had no audible effect");
        CHECK (! runningAfter, "module did not return to idle");
    }

    std::printf("== Scenes ==\n");
    {
        std::vector<Scene> all = transitionScenes();
        for (auto& s : presetScenes()) all.push_back (s);
        for (auto& sc : all)
        {
            TransitFxProcessor p; p.prepareToPlay (sr, bs);
            Gen gen { sr }; juce::MidiBuffer mb; juce::AudioBuffer<float> in (2, bs), buf (2, bs);
            p.applyScene (sc);
            Stats st;
            for (int k = 0; k < (int) (4.0 * sr / bs); ++k) { gen.fill (in); buf.makeCopyOf (in); p.processBlock (buf, mb); accumulate (buf, in, st); }
            std::printf("  %-20s peak %.2f diff %.3f\n", sc.name.toRawUTF8(), st.peak, st.diff / (double) st.n);
            CHECK (! st.bad && st.peak < 8.f, "scene output invalid");
            CHECK (st.diff / (double) st.n > 0.002, "scene silent / unchanged");
        }
    }

    std::printf("== Recorder + WAV export ==\n");
    {
        TransitFxProcessor p; p.prepareToPlay (sr, bs);
        Gen gen { sr }; juce::MidiBuffer mb; juce::AudioBuffer<float> in (2, bs), buf (2, bs);
        setP (p, "delay_on", 1.f); setP (p, "delay_mode", 0.f); setP (p, "delay_feedback", 70.f);
        auto& rec = p.getRecorder();
        p.recordStart();
        for (int k = 0; k < (int) (2.0 * sr / bs); ++k) { gen.fill (in); buf.makeCopyOf (in); p.processBlock (buf, mb); }
        const int lenAtStop = rec.getLength();
        p.recordStop();
        CHECK (rec.getState() == TransitRecorder::State::Finishing, "should be capturing tail");
        setP (p, "delay_on", 0.f);
        for (int k = 0; k < (int) (6.0 * sr / bs) && rec.getState() != TransitRecorder::State::Ready; ++k)
        { in.clear(); buf.makeCopyOf (in); p.processBlock (buf, mb); }
        CHECK (rec.getState() == TransitRecorder::State::Ready, "recording did not finish");
        std::printf("  length at stop %.2fs, final %.2fs\n", lenAtStop / sr, rec.getLength() / sr);
        CHECK (rec.getLength() > lenAtStop, "tail was not captured");

        auto f1 = rec.getOrCreateFile (0);
        auto f1b = rec.getOrCreateFile (0);
        auto f2 = rec.getOrCreateFile (44100);
        std::printf("  %s\n  %s\n", f1.getFullPathName().toRawUTF8(), f2.getFullPathName().toRawUTF8());
        CHECK (f1.existsAsFile() && f1 == f1b, "file cache");
        CHECK (f2.existsAsFile() && f2 != f1, "unique filename per render");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> rd (wav.createReaderFor (f1.createInputStream().release(), true));
        CHECK (rd != nullptr && rd->bitsPerSample == 24 && rd->numChannels == 2 && (int) rd->sampleRate == 48000, "24-bit stereo wav");
        if (rd) CHECK ((juce::int64) rd->lengthInSamples == rec.getLength(), "wav length");
        std::unique_ptr<juce::AudioFormatReader> rd2 (wav.createReaderFor (f2.createInputStream().release(), true));
        CHECK (rd2 != nullptr && (int) rd2->sampleRate == 44100, "resampled wav");
        CHECK (f1.getFileName().startsWith ("STEINEX_TRANSITFX_"), "file name pattern");

        p.getRecorder().startPreview();
        buf.clear (); p.processBlock (buf, mb);
        CHECK (buf.getMagnitude (0, bs) > 0.f, "preview audio");
        rec.clear();
        CHECK (rec.getState() == TransitRecorder::State::Empty, "delete");
    }

    std::printf("== State round trip ==\n");
    {
        TransitFxProcessor a; a.prepareToPlay (sr, bs);
        setP (a, "stutter_gate", 33.f);
        a.setOrder ({ 3, 1, 2, 0, 4, 5, 6, 7, 8 });
        juce::MemoryBlock mbk; a.getStateInformation (mbk);
        TransitFxProcessor b; b.prepareToPlay (sr, bs); b.setStateInformation (mbk.getData(), (int) mbk.getSize());
        CHECK (std::abs (b.apvts.getRawParameterValue ("stutter_gate")->load() - 33.f) < 0.2f, "param restore");
        CHECK (b.getOrder()[0] == 3 && b.getOrder()[3] == 0, "order restore");
        CHECK (a.saveUserPreset ("Self Test") && a.loadUserPreset (TransitFxProcessor::presetsDir().getChildFile ("Self Test.steinexpreset")), "user preset");
        TransitFxProcessor::presetsDir().getChildFile ("Self Test.steinexpreset").deleteFile();
    }

    std::printf("== UI snapshot ==\n");
    {
        TransitFxProcessor p; p.prepareToPlay (sr, bs);
        setP (p, "backverb_on", 1.f); setP (p, "riser_on", 1.f);
        Gen gen { sr }; juce::MidiBuffer mb; juce::AudioBuffer<float> in (2, bs), buf (2, bs);
        p.recordStart();
        for (int k = 0; k < (int) (3.0 * sr / bs); ++k) { gen.fill (in); buf.makeCopyOf (in); p.processBlock (buf, mb); }
        setP (p, "rec_tail", 0.f); p.recordStop();
        std::unique_ptr<TransitFxEditor> ed (static_cast<TransitFxEditor*> (p.createEditor()));
        ed->setSize (1100, 720);
        auto img = ed->renderSnapshot (1.0f);
        juce::File out ("/home/claude/ui_snapshot.png");
        out.deleteFile();
        juce::PNGImageFormat png; juce::FileOutputStream fos (out);
        CHECK (png.writeImageToStream (img, fos), "png");
        std::printf("  wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        ed.reset();
    }

    std::printf(failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
