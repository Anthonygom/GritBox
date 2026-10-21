#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
APVTS::ParameterLayout GritBoxAudioProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout layout;

    auto pid = [] (const char* id) { return ParameterID { id, 1 }; };

    layout.add (std::make_unique<AudioParameterChoice> (pid ("mode"), "Mode",
                                                        StringArray { "Crunch", "Lead", "Fuzz" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("drive"), "Drive",
                                                       NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.5f));

    NormalisableRange<float> tightRange (40.0f, 400.0f, 1.0f);
    tightRange.setSkewForCentre (120.0f);
    layout.add (std::make_unique<AudioParameterFloat> (pid ("tight"), "Tight", tightRange, 110.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("tone"), "Tone",
                                                       NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("gate"), "Gate",
                                                       NormalisableRange<float> (-80.0f, -30.0f, 0.1f), -80.0f,
                                                       AudioParameterFloatAttributes()
                                                           .withLabel ("dB")
                                                           .withStringFromValueFunction ([] (float v, int)
                                                           {
                                                               return v <= -79.0f ? String ("Off") : String (v, 1) + " dB";
                                                           })));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("level"), "Level",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (pid ("mix"), "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (pid ("cab"), "Cab Sim", true));

    return layout;
}

//==============================================================================
GritBoxAudioProcessor::GritBoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRITBOX", createLayout())
{
    drive = apvts.getRawParameterValue ("drive");
    tight = apvts.getRawParameterValue ("tight");
    tone  = apvts.getRawParameterValue ("tone");
    gate  = apvts.getRawParameterValue ("gate");
    level = apvts.getRawParameterValue ("level");
    mix   = apvts.getRawParameterValue ("mix");
    mode  = apvts.getRawParameterValue ("mode");
    cab   = apvts.getRawParameterValue ("cab");
}

bool GritBoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void GritBoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void GritBoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    GritParams p;
    p.driveDb = drive->load() * 5.0f;                 // knob 0..10  ->  0..50 dB of gain
    p.tightHz = tight->load();
    p.tone    = tone->load() / 10.0f;
    p.gateDb  = gate->load();
    p.levelDb = level->load();
    p.mix     = mix->load() / 100.0f;
    p.mode    = (int) mode->load();
    p.cab     = cab->load() > 0.5f;

    engine.process (buffer, p);
}

//==============================================================================
juce::AudioProcessorEditor* GritBoxAudioProcessor::createEditor()
{
    return new GritBoxAudioProcessorEditor (*this);
}

void GritBoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GritBoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GritBoxAudioProcessor();
}
