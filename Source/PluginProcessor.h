#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Params.h"
#include "Dsp.h"
#include "Recorder.h"
#include "Scenes.h"

class TransitFxProcessor : public juce::AudioProcessor
{
public:
    TransitFxProcessor();
    ~TransitFxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TRANSIT FX"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    steinex::TransitRecorder& getRecorder() { return recorder; }

    // effect chain order (slot -> module id)
    std::array<int, steinex::kNumModules> getOrder() const;
    void setOrder (const std::array<int, steinex::kNumModules>& o);

    bool isModuleRunning (int m) const { return modules[(size_t) m]->isRunning(); }
    float getMeterL() const { return meterL.load(); }
    float getMeterR() const { return meterR.load(); }

    // scenes / presets
    void applyScene (const steinex::Scene& s);
    void turnAllModulesOff();
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    static juce::File presetsDir();
    static juce::Array<juce::File> listUserPresets();

    // recorder helpers (message thread)
    void recordStart();
    void recordStop();
    int  getExportSampleRate() const;

private:
    void setParamReal (const juce::String& id, float realValue);
    void resetModuleParams();

    std::array<std::unique_ptr<steinex::fx::ModuleBase>, steinex::kNumModules> modules;
    std::atomic<juce::uint64> orderPacked { 0 };
    steinex::TransitRecorder recorder;

    juce::AudioBuffer<float> dryBuf;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pTail = nullptr;
    std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pRecSR = nullptr;

    juce::SmoothedValue<float> mixSm, outSm;
    std::atomic<float> meterL { 0.f }, meterR { 0.f };
    double currentSR = 44100.0;
    std::atomic<bool> loggedFirstBlock { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransitFxProcessor)
};
