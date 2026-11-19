#include "PluginEditor.h"

VoltageStarveAudioProcessorEditor::VoltageStarveAudioProcessorEditor(VoltageStarveAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setupKnob(starveSlider, starveLabel, "STARVE");
    setupKnob(dragSlider,   dragLabel,   "DRAG");
    setupKnob(envSlider,    envLabel,    "ENV");

    starveAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "starve", starveSlider);
    dragAttachment   = std::make_unique<Attachment>(audioProcessor.apvts, "drag",   dragSlider);
    envAttachment    = std::make_unique<Attachment>(audioProcessor.apvts, "env",    envSlider);

    setSize(340, 160);
}

void VoltageStarveAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFE07A5F));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(slider);

    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xFFCCCCCC));
    addAndMakeVisible(label);
}

void VoltageStarveAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF18181B));

    g.setColour(juce::Colour(0xFF888890));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("VOLTAGE STARVE", getLocalBounds().removeFromTop(28), juce::Justification::centred);
}

void VoltageStarveAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(20);

    const int colWidth = area.getWidth() / 3;

    auto layoutKnob = [&](juce::Slider& s, juce::Label& l)
    {
        auto col = area.removeFromLeft(colWidth);
        l.setBounds(col.removeFromTop(18));
        s.setBounds(col);
    };

    layoutKnob(starveSlider, starveLabel);
    layoutKnob(dragSlider,   dragLabel);
    layoutKnob(envSlider,    envLabel);
}