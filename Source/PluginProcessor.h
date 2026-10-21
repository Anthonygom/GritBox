#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GritEngine.h"

class GritBoxAudioProcessor final : public juce::AudioProcessor
{
public:
    GritBoxAudioProcessor();
    ~GritBoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GritBox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    GritEngine engine;

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* tight = nullptr;
    std::atomic<float>* tone  = nullptr;
    std::atomic<float>* gate  = nullptr;
    std::atomic<float>* level = nullptr;
    std::atomic<float>* mix   = nullptr;
    std::atomic<float>* mode  = nullptr;
    std::atomic<float>* cab   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GritBoxAudioProcessor)
};
