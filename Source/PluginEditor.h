#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class ClickLogger : public juce::MouseListener
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    int n = 0;
};

class TransitFxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TransitFxEditor (TransitFxProcessor&);
    ~TransitFxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Renders the whole UI to an image (used by the build test)
    juce::Image renderSnapshot (float scale = 1.0f);

private:
    void timerCallback() override;
    int tries = 0;
    ClickLogger clickLogger;
    bool firstPaint = true;
    TransitFxProcessor& proc;
    std::unique_ptr<juce::Component> content;
    static constexpr int baseW = 1100, baseH = 720;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransitFxEditor)
};
