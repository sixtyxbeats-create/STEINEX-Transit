#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

// STEINEX TRANSIT FX - parameter table.
// One table drives the DSP, the host parameter layout and the UI controls.

namespace steinex
{
enum ModuleId { kStutter = 0, kBackverb, kDelay, kTape, kPitch, kGlitch, kFreeze, kRiser, kRevDelay, kNumModules };

//==============================================================================
struct ParamSpec
{
    enum Kind { Float, Choice, Bool };
    juce::String id, name, unit;
    Kind kind = Float;
    float min = 0.f, max = 1.f, def = 0.f, step = 0.f, skew = 1.f;
    juce::StringArray choices;
};

inline ParamSpec F (const juce::String& id, const juce::String& name, float mn, float mx, float def,
                    const juce::String& unit = {}, float skew = 1.f, float step = 0.f)
{
    ParamSpec p; p.id = id; p.name = name; p.kind = ParamSpec::Float;
    p.min = mn; p.max = mx; p.def = def; p.unit = unit; p.skew = skew; p.step = step;
    return p;
}
inline ParamSpec C (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int def)
{
    ParamSpec p; p.id = id; p.name = name; p.kind = ParamSpec::Choice; p.choices = choices; p.def = (float) def;
    p.min = 0.f; p.max = (float) (choices.size() - 1);
    return p;
}
inline ParamSpec B (const juce::String& id, const juce::String& name, bool def)
{
    ParamSpec p; p.id = id; p.name = name; p.kind = ParamSpec::Bool; p.def = def ? 1.f : 0.f; p.max = 1.f;
    return p;
}

//==============================================================================
// Musical divisions
inline const juce::StringArray& stutterDivNames()
{
    static const juce::StringArray a { "1/2", "1/4", "1/8", "1/16", "1/32" };
    return a;
}
inline double stutterDivBeats (int i)
{
    static const double b[] { 2.0, 1.0, 0.5, 0.25, 0.125 };
    return b[juce::jlimit (0, 4, i)];
}

inline const juce::StringArray& timeNames()
{
    static const juce::StringArray a { "1/32", "1/16", "1/16T", "1/8", "1/8 dot", "1/8T",
                                       "1/4", "1/4 dot", "1/4T", "1/2", "1 bar", "2 bars" };
    return a;
}
inline double timeBeats (int i)
{
    static const double b[] { 0.125, 0.25, 1.0 / 6.0, 0.5, 0.75, 1.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 2.0, 4.0, 8.0 };
    return b[juce::jlimit (0, 11, i)];
}

inline const juce::StringArray& riserLenNames()
{
    static const juce::StringArray a { "0.5 s", "1 s", "2 s", "4 s", "8 s", "1 bar", "2 bars", "4 bars", "8 bars" };
    return a;
}
inline double riserSeconds (int i, double bpm)
{
    static const double s[] { 0.5, 1.0, 2.0, 4.0, 8.0 };
    const double bar = 4.0 * 60.0 / juce::jmax (20.0, bpm);
    if (i < 5) return s[juce::jmax (0, i)];
    static const double bars[] { 1.0, 2.0, 4.0, 8.0 };
    return bar * bars[juce::jlimit (0, 3, i - 5)];
}

inline const juce::StringArray& glitchPatternNames()
{
    static const juce::StringArray a { "Steady", "Off-beat", "3-3-2", "Skip", "Random" };
    return a;
}

//==============================================================================
// Parameter index enums - they MUST follow the order of the spec lists below.
namespace st { enum { Div, Speed, Mode, Repeats, Gate, Smooth, Intensity, Count }; }
namespace bv { enum { Size, Decay, Sync, Time, PreDelay, Tail, Mix, Count }; }
namespace dl { enum { Time, Mode, Send, Feedback, Width, Hpf, Lpf, Mix, Count }; }
namespace tp { enum { Time, Curve, Depth, Count }; }
namespace ps { enum { Mode, Range, Speed, Window, Mix, Count }; }
namespace gl { enum { Rate, Pattern, Grain, Intensity, Reverse, Random, Count }; }
namespace fz { enum { Mode, Hold, Size, Damp, Release, Mix, Count }; }
namespace rs { enum { Mode, Len, Noise, Tone, Filter, Intensity, Count }; }
namespace rd { enum { Time, Feedback, Stereo, Hpf, Lpf, Mix, Count }; }

struct ModuleSpec
{
    juce::String key, name, tag;
    juce::uint32 colour;
    std::vector<ParamSpec> params;
};

inline const std::array<ModuleSpec, kNumModules>& moduleSpecs()
{
    static const std::array<ModuleSpec, kNumModules> specs = []
    {
        std::array<ModuleSpec, kNumModules> s;

        s[kStutter] = { "stutter", "STUTTER", "Rhythmic repeat and chop", 0xff35d7ff, {
            C ("div",   "Division",  stutterDivNames(), 3),
            C ("speed", "Speed",     { "1x", "2x", "4x" }, 0),
            C ("mode",  "Direction", { "Forward", "Reverse", "Alternate" }, 0),
            F ("repeats",   "Repeats",   1.f, 8.f, 4.f, "x", 1.f, 1.f),
            F ("gate",      "Gate",      10.f, 100.f, 70.f, "%"),
            F ("smooth",    "Smooth",    0.5f, 20.f, 3.f, "ms", 0.5f),
            F ("intensity", "Intensity", 0.f, 100.f, 100.f, "%") } };

        s[kBackverb] = { "backverb", "BACKVERB", "Reverse reverb swell", 0xff8a63ff, {
            F ("size",   "Size",       0.f, 100.f, 60.f, "%"),
            F ("decay",  "Decay",      0.2f, 6.f, 1.5f, "s", 0.6f),
            B ("sync",   "Tempo Sync", false),
            C ("time",   "Sync Time",  timeNames(), 9),
            F ("predelay", "Pre-Delay", 0.f, 200.f, 20.f, "ms"),
            F ("tail",   "Tail",       0.f, 200.f, 100.f, "%"),
            F ("mix",    "Wet/Dry",    0.f, 100.f, 70.f, "%") } };

        s[kDelay] = { "delay", "DELAY THROW", "Tempo delay and last-word throw", 0xff4d8dff, {
            C ("time",  "Time",  timeNames(), 6),
            C ("mode",  "Mode",  { "Continuous", "Throw" }, 0),
            C ("send",  "Throw Length", timeNames(), 6),
            F ("feedback", "Feedback", 0.f, 95.f, 45.f, "%"),
            F ("width",    "Width",    0.f, 100.f, 60.f, "%"),
            F ("hpf",      "High-Pass", 20.f, 2000.f, 120.f, "Hz", 0.35f),
            F ("lpf",      "Low-Pass",  500.f, 20000.f, 7500.f, "Hz", 0.35f),
            F ("mix",      "Wet/Dry",   0.f, 100.f, 50.f, "%") } };

        s[kTape] = { "tape", "TAPE STOP", "Tape slowdown with pitch drop", 0xffffb547, {
            F ("time",  "Stop Time", 0.1f, 4.f, 1.2f, "s", 0.6f),
            C ("curve", "Curve",     { "Linear", "Ease-in", "Ease-out", "S-curve" }, 0),
            F ("depth", "Depth",     0.f, 100.f, 100.f, "%") } };

        s[kPitch] = { "pitch", "PITCH", "Pitch rise and drop", 0xffff3d8b, {
            C ("mode",   "Direction", { "Rise", "Drop" }, 0),
            F ("range",  "Range",     1.f, 24.f, 12.f, "st", 1.f, 1.f),
            F ("speed",  "Speed",     0.05f, 8.f, 1.5f, "s", 0.5f),
            F ("window", "Smoothness", 20.f, 100.f, 50.f, "ms"),
            F ("mix",    "Wet/Dry",   0.f, 100.f, 100.f, "%") } };

        s[kGlitch] = { "glitch", "GLITCH", "Granular rhythmic glitch", 0xff3de8a0, {
            C ("rate",    "Rate",    timeNames(), 1),
            C ("pattern", "Pattern", glitchPatternNames(), 0),
            F ("grain",     "Grain Size", 5.f, 200.f, 60.f, "ms", 0.5f),
            F ("intensity", "Intensity",  0.f, 100.f, 70.f, "%"),
            F ("reverse",   "Reverse",    0.f, 100.f, 30.f, "%"),
            F ("random",    "Random",     0.f, 100.f, 40.f, "%") } };

        s[kFreeze] = { "freeze", "REVERB FREEZE", "Infinite reverb sustain", 0xff7fe7ff, {
            C ("mode",    "Sustain", { "Infinite", "Timed" }, 0),
            F ("hold",    "Hold",    0.5f, 8.f, 3.f, "s", 0.6f),
            F ("size",    "Size",    0.f, 100.f, 85.f, "%"),
            F ("damp",    "Damping", 0.f, 100.f, 35.f, "%"),
            F ("release", "Release", 0.1f, 10.f, 2.5f, "s", 0.5f),
            F ("mix",     "Wet/Dry", 0.f, 100.f, 80.f, "%") } };

        s[kRiser] = { "riser", "RISER", "Risers and downlifters", 0xffff7a45, {
            C ("mode", "Type",     { "Riser", "Downlifter" }, 0),
            C ("len",  "Duration", riserLenNames(), 3),
            F ("noise",     "Noise",     0.f, 100.f, 70.f, "%"),
            F ("tone",      "Pitch Sweep", 0.f, 100.f, 40.f, "%"),
            F ("filter",    "Filter Sweep", 0.f, 100.f, 70.f, "%"),
            F ("intensity", "Intensity", 0.f, 100.f, 60.f, "%") } };

        s[kRevDelay] = { "revdelay", "REVERSE DELAY", "Reverse echo repeats", 0xffc06cff, {
            C ("time",  "Time", timeNames(), 6),
            F ("feedback", "Feedback", 0.f, 90.f, 40.f, "%"),
            F ("stereo",   "Stereo",   0.f, 100.f, 50.f, "%"),
            F ("hpf",      "High-Pass", 20.f, 2000.f, 120.f, "Hz", 0.35f),
            F ("lpf",      "Low-Pass",  500.f, 20000.f, 7500.f, "Hz", 0.35f),
            F ("mix",      "Wet/Dry",   0.f, 100.f, 50.f, "%") } };

        return s;
    }();
    return specs;
}

inline juce::String paramId (int module, const juce::String& id)  { return moduleSpecs()[(size_t) module].key + "_" + id; }
inline juce::String onId (int module)                              { return moduleSpecs()[(size_t) module].key + "_on"; }

//==============================================================================
inline juce::String fmtValue (float v, const juce::String& unit)
{
    if (unit == "%")  return juce::String (juce::roundToInt (v)) + "%";
    if (unit == "ms") return (v < 10.f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms";
    if (unit == "Hz") return v >= 1000.f ? juce::String (v / 1000.f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    if (unit == "s")  return juce::String (v, 2) + " s";
    if (unit == "st") return juce::String (juce::roundToInt (v)) + " st";
    if (unit == "dB") return juce::String (v, 1) + " dB";
    if (unit == "x")  return juce::String (juce::roundToInt (v)) + "x";
    return juce::String (v, 2);
}

// Global parameters
inline const std::vector<ParamSpec>& globalSpecs()
{
    static const std::vector<ParamSpec> g {
        F ("g_mix",    "Wet/Dry", 0.f, 100.f, 100.f, "%"),
        F ("g_out",    "Output",  -24.f, 6.f, 0.f, "dB"),
        B ("g_bypass", "Bypass",  false),
        C ("rec_sr",   "Export Sample Rate", { "Host", "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" }, 0),
        F ("rec_tail", "Tail Capture", 0.f, 8.f, 3.f, "s", 0.6f),
        B ("rec_trim", "Auto-Trim", true)
    };
    return g;
}

inline void addSpec (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const ParamSpec& p, const juce::String& id)
{
    using namespace juce;
    if (p.kind == ParamSpec::Float)
    {
        const String unit = p.unit;
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, p.name,
            NormalisableRange<float> (p.min, p.max, p.step, p.skew), p.def,
            AudioParameterFloatAttributes().withLabel (unit)
                .withStringFromValueFunction ([unit] (float v, int) { return fmtValue (v, unit); })));
    }
    else if (p.kind == ParamSpec::Choice)
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, p.name, p.choices, (int) p.def));
    else
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, p.name, p.def > 0.5f));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto& g : globalSpecs())
        addSpec (layout, g, g.id);

    for (int m = 0; m < kNumModules; ++m)
    {
        const auto& ms = moduleSpecs()[(size_t) m];
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { onId (m), 1 }, ms.name + " On", false));
        for (auto& p : ms.params)
            addSpec (layout, p, paramId (m, p.id));
    }
    return layout;
}
} // namespace steinex
