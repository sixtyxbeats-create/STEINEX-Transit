#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// STEINEX TRANSIT FX - factory scenes.
// A scene lists parameter values by ID. Choice parameters use their index.
// Applying a scene first resets every module to its defaults, then sets the listed values.

namespace steinex
{
struct SceneVal { juce::String id; float v; };
struct Scene    { juce::String name; std::vector<SceneVal> vals; };

// time list indices: 0 1/32, 1 1/16, 2 1/16T, 3 1/8, 4 1/8 dot, 5 1/8T, 6 1/4, 7 1/4 dot, 8 1/4T, 9 1/2, 10 1 bar, 11 2 bars
// riser length indices: 0 0.5s, 1 1s, 2 2s, 3 4s, 4 8s, 5 1 bar, 6 2 bars, 7 4 bars, 8 8 bars

inline const std::vector<Scene>& transitionScenes()
{
    static const std::vector<Scene> s {
        { "VOCAL STUTTER", {
            { "stutter_on", 1 }, { "stutter_div", 3 }, { "stutter_speed", 0 }, { "stutter_mode", 0 },
            { "stutter_repeats", 4 }, { "stutter_gate", 65 }, { "stutter_smooth", 2.5f }, { "stutter_intensity", 100 },
            { "delay_on", 1 }, { "delay_mode", 0 }, { "delay_time", 3 }, { "delay_feedback", 30 }, { "delay_mix", 25 } } },

        { "REVERSE VOCAL", {
            { "backverb_on", 1 }, { "backverb_size", 70 }, { "backverb_decay", 2.0f }, { "backverb_sync", 0 },
            { "backverb_predelay", 10 }, { "backverb_tail", 120 }, { "backverb_mix", 85 } } },

        { "LAST WORD THROW", {
            { "delay_on", 1 }, { "delay_mode", 1 }, { "delay_send", 6 }, { "delay_time", 7 },
            { "delay_feedback", 55 }, { "delay_width", 80 }, { "delay_hpf", 200 }, { "delay_lpf", 6500 }, { "delay_mix", 65 } } },

        { "CHORUS ENTRANCE", {
            { "backverb_on", 1 }, { "backverb_sync", 1 }, { "backverb_time", 10 }, { "backverb_size", 80 },
            { "backverb_predelay", 10 }, { "backverb_tail", 135 }, { "backverb_mix", 80 },
            { "riser_on", 1 }, { "riser_mode", 0 }, { "riser_len", 5 }, { "riser_noise", 60 }, { "riser_tone", 30 },
            { "riser_filter", 80 }, { "riser_intensity", 45 } } },

        { "DARK TRANSITION", {
            { "pitch_on", 1 }, { "pitch_mode", 1 }, { "pitch_range", 12 }, { "pitch_speed", 2.0f }, { "pitch_mix", 100 },
            { "freeze_on", 1 }, { "freeze_mode", 0 }, { "freeze_size", 92 }, { "freeze_damp", 70 }, { "freeze_release", 3.0f }, { "freeze_mix", 65 },
            { "delay_on", 1 }, { "delay_mode", 0 }, { "delay_time", 7 }, { "delay_feedback", 50 }, { "delay_lpf", 2800 }, { "delay_mix", 35 } } },

        { "TAPE FALL", {
            { "tape_on", 1 }, { "tape_time", 1.6f }, { "tape_curve", 1 }, { "tape_depth", 100 } } },

        { "GLITCH DROP", {
            { "glitch_on", 1 }, { "glitch_rate", 1 }, { "glitch_pattern", 2 }, { "glitch_grain", 45 },
            { "glitch_intensity", 85 }, { "glitch_reverse", 40 }, { "glitch_random", 55 },
            { "revdelay_on", 1 }, { "revdelay_time", 3 }, { "revdelay_feedback", 35 }, { "revdelay_stereo", 70 }, { "revdelay_mix", 35 } } },

        { "REVERB FREEZE", {
            { "freeze_on", 1 }, { "freeze_mode", 0 }, { "freeze_size", 90 }, { "freeze_damp", 40 },
            { "freeze_release", 2.5f }, { "freeze_mix", 85 } } },

        { "RAP TRANSITION", {
            { "stutter_on", 1 }, { "stutter_div", 2 }, { "stutter_speed", 0 }, { "stutter_mode", 0 },
            { "stutter_repeats", 3 }, { "stutter_gate", 60 }, { "stutter_smooth", 2 }, { "stutter_intensity", 100 },
            { "delay_on", 1 }, { "delay_mode", 1 }, { "delay_send", 3 }, { "delay_time", 3 },
            { "delay_feedback", 40 }, { "delay_width", 60 }, { "delay_mix", 55 } } },

        { "FULL STOP", {
            { "tape_on", 1 }, { "tape_time", 0.7f }, { "tape_curve", 2 }, { "tape_depth", 100 },
            { "freeze_on", 1 }, { "freeze_mode", 1 }, { "freeze_hold", 2.0f }, { "freeze_size", 80 },
            { "freeze_release", 1.5f }, { "freeze_mix", 60 } } }
    };
    return s;
}

inline const std::vector<Scene>& presetScenes()
{
    static const std::vector<Scene> s {
        { "Vocal Chop", {
            { "stutter_on", 1 }, { "stutter_div", 3 }, { "stutter_repeats", 2 }, { "stutter_gate", 55 }, { "stutter_smooth", 2 },
            { "glitch_on", 1 }, { "glitch_rate", 1 }, { "glitch_pattern", 3 }, { "glitch_grain", 40 },
            { "glitch_intensity", 45 }, { "glitch_reverse", 20 }, { "glitch_random", 30 } } },

        { "Dark Reverse", {
            { "backverb_on", 1 }, { "backverb_size", 85 }, { "backverb_decay", 2.5f }, { "backverb_predelay", 15 },
            { "backverb_tail", 130 }, { "backverb_mix", 80 },
            { "freeze_on", 1 }, { "freeze_size", 90 }, { "freeze_damp", 70 }, { "freeze_release", 3 }, { "freeze_mix", 45 },
            { "delay_on", 1 }, { "delay_time", 7 }, { "delay_feedback", 45 }, { "delay_width", 70 },
            { "delay_hpf", 250 }, { "delay_lpf", 3200 }, { "delay_mix", 35 } } },

        { "Rap Transition", {
            { "stutter_on", 1 }, { "stutter_div", 2 }, { "stutter_repeats", 3 }, { "stutter_gate", 60 },
            { "delay_on", 1 }, { "delay_mode", 1 }, { "delay_send", 3 }, { "delay_time", 3 },
            { "delay_feedback", 40 }, { "delay_mix", 55 } } },

        { "Last Word Throw", {
            { "delay_on", 1 }, { "delay_mode", 1 }, { "delay_send", 6 }, { "delay_time", 7 }, { "delay_feedback", 58 },
            { "delay_width", 85 }, { "delay_hpf", 220 }, { "delay_lpf", 6000 }, { "delay_mix", 70 },
            { "backverb_on", 1 }, { "backverb_size", 40 }, { "backverb_decay", 0.8f }, { "backverb_tail", 60 }, { "backverb_mix", 20 } } },

        { "Chorus Entrance", {
            { "backverb_on", 1 }, { "backverb_sync", 1 }, { "backverb_time", 10 }, { "backverb_size", 80 },
            { "backverb_tail", 140 }, { "backverb_mix", 85 },
            { "riser_on", 1 }, { "riser_len", 5 }, { "riser_noise", 55 }, { "riser_tone", 35 }, { "riser_filter", 80 }, { "riser_intensity", 45 } } },

        { "Beat Glitch", {
            { "glitch_on", 1 }, { "glitch_rate", 1 }, { "glitch_pattern", 2 }, { "glitch_grain", 55 },
            { "glitch_intensity", 85 }, { "glitch_reverse", 35 }, { "glitch_random", 60 },
            { "stutter_on", 1 }, { "stutter_div", 4 }, { "stutter_mode", 2 }, { "stutter_repeats", 2 },
            { "stutter_gate", 50 }, { "stutter_smooth", 1.5f }, { "stutter_intensity", 55 } } },

        { "Tape Fall", {
            { "tape_on", 1 }, { "tape_time", 1.6f }, { "tape_curve", 1 }, { "tape_depth", 100 },
            { "delay_on", 1 }, { "delay_time", 7 }, { "delay_feedback", 50 }, { "delay_width", 70 },
            { "delay_hpf", 200 }, { "delay_lpf", 4500 }, { "delay_mix", 25 } } },

        { "Reverse Atmosphere", {
            { "backverb_on", 1 }, { "backverb_size", 95 }, { "backverb_decay", 3.5f }, { "backverb_predelay", 40 },
            { "backverb_tail", 120 }, { "backverb_mix", 65 },
            { "revdelay_on", 1 }, { "revdelay_time", 6 }, { "revdelay_feedback", 55 }, { "revdelay_stereo", 80 },
            { "revdelay_hpf", 180 }, { "revdelay_lpf", 5500 }, { "revdelay_mix", 45 } } },

        { "Stutter Drop", {
            { "stutter_on", 1 }, { "stutter_div", 3 }, { "stutter_speed", 1 }, { "stutter_mode", 2 },
            { "stutter_repeats", 4 }, { "stutter_gate", 70 }, { "stutter_smooth", 2 },
            { "tape_on", 1 }, { "tape_time", 1.0f }, { "tape_curve", 3 }, { "tape_depth", 100 } } },

        { "Freeze & Release", {
            { "freeze_on", 1 }, { "freeze_mode", 1 }, { "freeze_hold", 2.5f }, { "freeze_size", 90 },
            { "freeze_damp", 40 }, { "freeze_release", 3 }, { "freeze_mix", 85 } } },

        { "Underground", {
            { "pitch_on", 1 }, { "pitch_mode", 1 }, { "pitch_range", 5 }, { "pitch_speed", 2.5f }, { "pitch_window", 60 },
            { "delay_on", 1 }, { "delay_time", 7 }, { "delay_feedback", 50 }, { "delay_lpf", 2500 }, { "delay_mix", 40 },
            { "freeze_on", 1 }, { "freeze_mode", 1 }, { "freeze_hold", 3 }, { "freeze_size", 85 },
            { "freeze_damp", 75 }, { "freeze_mix", 35 } } },

        { "Hyper Glitch", {
            { "glitch_on", 1 }, { "glitch_rate", 0 }, { "glitch_pattern", 4 }, { "glitch_grain", 25 },
            { "glitch_intensity", 100 }, { "glitch_reverse", 50 }, { "glitch_random", 85 },
            { "stutter_on", 1 }, { "stutter_div", 4 }, { "stutter_speed", 2 }, { "stutter_mode", 2 },
            { "stutter_repeats", 3 }, { "stutter_gate", 45 }, { "stutter_smooth", 1 }, { "stutter_intensity", 70 },
            { "pitch_on", 1 }, { "pitch_mode", 0 }, { "pitch_range", 12 }, { "pitch_speed", 2 }, { "pitch_window", 40 }, { "pitch_mix", 70 } } }
    };
    return s;
}
} // namespace steinex
