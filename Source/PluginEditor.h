#pragma once

#include "PluginProcessor.h"

class VoltageStarveAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoltageStarveAudioProcessorEditor(VoltageStarveAudioProcessor&);
    ~VoltageStarveAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& name);

    VoltageStarveAudioProcessor& audioProcessor;

    juce::Slider starveSlider, dragSlider, envSlider;
    juce::Label  starveLabel,  dragLabel,  envLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> starveAttachment, dragAttachment, envAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoltageStarveAudioProcessorEditor)
};