#include "PluginEditor.h"

//==============================================================================
GritLookAndFeel::GritLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colour (0xffb8b8c0));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff25252b));
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a42));
    setColour (juce::ComboBox::arrowColourId, accent());
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff25252b));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent());
    setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::tickColourId, accent());
    setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff3a3a42));
}

void GritLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle,
                                        juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcW = 5.0f;
    const float arcR = radius - arcW * 0.5f;

    // knob body
    g.setColour (juce::Colour (0xff25252b));
    g.fillEllipse (centre.x - radius + 8.0f, centre.y - radius + 8.0f, (radius - 8.0f) * 2.0f, (radius - 8.0f) * 2.0f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff35353d));
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent());
    g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    const float pLen = radius * 0.45f;
    pointer.addRoundedRectangle (-1.75f, -(radius - 12.0f), 3.5f, pLen, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colours::white);
    g.fillPath (pointer);
}

//==============================================================================
GritBoxAudioProcessorEditor::GritBoxAudioProcessorEditor (GritBoxAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    addKnob (driveKnob, "drive", "DRIVE");
    addKnob (tightKnob, "tight", "TIGHT");
    addKnob (toneKnob,  "tone",  "TONE");
    addKnob (gateKnob,  "gate",  "GATE");
    addKnob (levelKnob, "level", "LEVEL");
    addKnob (mixKnob,   "mix",   "MIX");

    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeLabel);

    modeBox.addItemList ({ "Crunch", "Lead", "Fuzz" }, 1);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, "mode", modeBox);

    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "cab", cabButton);

    setSize (620, 300);
}

GritBoxAudioProcessorEditor::~GritBoxAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GritBoxAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setDoubleClickReturnValue (true, audioProcessor.apvts.getParameter (paramId)->convertFrom0to1 (
                                                  audioProcessor.apvts.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);
}

void GritBoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (GritLookAndFeel::background());

    g.setColour (GritLookAndFeel::accent());
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("GRITBOX", 20, 12, 300, 36, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff8a8a94));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("guitar distortion", 22, 44, 300, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff2c2c33));
    g.drawLine (20.0f, 68.0f, (float) getWidth() - 20.0f, 68.0f, 1.0f);
}

void GritBoxAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (56);

    // top-right: mode + cab
    auto header = getLocalBounds().reduced (20).removeFromTop (50);
    cabButton.setBounds (header.removeFromRight (100).withSizeKeepingCentre (100, 24));
    header.removeFromRight (10);
    modeBox.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 28));
    modeLabel.setBounds (header.removeFromRight (50).withSizeKeepingCentre (50, 20));

    Knob* knobs[] = { &driveKnob, &tightKnob, &toneKnob, &gateKnob, &levelKnob, &mixKnob };
    const int w = area.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto col = area.removeFromLeft (w);
        knobs[i]->label.setBounds (col.removeFromTop (22));
        knobs[i]->slider.setBounds (col);
    }
}
