#pragma once

#include "PluginProcessor.h"

//==============================================================================
class GritLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GritLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    static juce::Colour accent() { return juce::Colour (0xffff6a1a); }
    static juce::Colour background() { return juce::Colour (0xff17171b); }
};

//==============================================================================
class GritBoxAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GritBoxAudioProcessorEditor (GritBoxAudioProcessor&);
    ~GritBoxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& name);

    GritBoxAudioProcessor& audioProcessor;
    GritLookAndFeel lookAndFeel;

    Knob driveKnob, tightKnob, toneKnob, gateKnob, levelKnob, mixKnob;

    juce::Label modeLabel;
    juce::ComboBox modeBox;
    std::unique_ptr<ComboAttachment> modeAttachment;

    juce::ToggleButton cabButton { "Cab Sim" };
    std::unique_ptr<ButtonAttachment> cabAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GritBoxAudioProcessorEditor)
};
