#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    inline double fract(double x)
    {
        return x - std::floor(x);
    }

    inline double mix(double a, double b, double t)
    {
        return a + (b - a) * t;
    }
}

VoltageStarveAudioProcessor::VoltageStarveAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    starveParam = apvts.getRawParameterValue("starve");
    dragParam   = apvts.getRawParameterValue("drag");
    envParam    = apvts.getRawParameterValue("env");
}

juce::AudioProcessorValueTreeState::ParameterLayout VoltageStarveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "starve", 1 }, "Starve",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drag", 1 }, "Drag",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "env", 1 }, "Env Load",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    return { params.begin(), params.end() };
}

void VoltageStarveAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Allocate at least 1 second (or 44100 samples) to support higher sample rates safely
    int maxDelaySamps = std::max(44100, static_cast<int>(std::ceil(currentSampleRate * 1.1)));
    dragL.prepare(maxDelaySamps);
    dragR.prepare(maxDelaySamps);

    sagCap  = 0.0;
    cogPh   = 0.0;
    humPh   = 0.0;
    gateEnv = 1.0;
    slewL   = 0.0;
    slewR   = 0.0;
    dcXL    = 0.0;
    dcYL    = 0.0;
    dcXR    = 0.0;
    dcYR    = 0.0;
}

bool VoltageStarveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() && in == juce::AudioChannelSet::stereo());
}

void VoltageStarveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    auto* leftChannel  = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    const double env     = std::clamp(static_cast<double>(envParam->load()), 0.0, 1.0);
    const double starve  = std::clamp(static_cast<double>(starveParam->load()), 0.0, 1.0);
    const double dragAmt = std::clamp(static_cast<double>(dragParam->load()), 0.0, 1.0);

    const double sr = currentSampleRate;
    const double maxDelLimit = std::min(44000.0, sr * 1.0);
    const double rDc = std::clamp(1.0 - (kTwoPi * 4.5 / sr), 0.998, 0.9999);
    const double wetMix = std::clamp(starve + dragAmt, 0.0, 1.0);
    const double makeup = 1.0 + starve * 0.38;

    for (int i = 0; i < numSamples; ++i)
    {
        const double inL = static_cast<double>(leftChannel[i]);
        const double inR = static_cast<double>(rightChannel[i]);

        dragL.write(inL);
        dragR.write(inR);

        // 1. Non-Linear Reservoir Capacitor Discharge & Slow Chemical Recovery
        const double loadDemand = std::clamp((std::abs(inL) + std::abs(inR)) * 0.72 + env * 0.65, 0.0, 1.5);
        const double capTarget  = loadDemand * std::clamp(starve * 0.85 + dragAmt * 0.45, 0.0, 1.0);
        const double capCoeff   = (capTarget > sagCap) ? 0.009 : 0.00010;
        sagCap += (capTarget - sagCap) * capCoeff;

        // 2. Inverse DC Brush Motor Cogging
        const double cogHz = std::clamp(7.6 - (sagCap * 4.2) - (dragAmt * 1.6), 1.8, 8.0);
        cogPh = fract(cogPh + (cogHz / sr));
        const double cogWave = (std::sin(cogPh * kTwoPi) * 0.5 + 0.5)
                             + 0.25 * (std::sin(cogPh * kTwoPi * 3.0) * 0.5 + 0.5);
        const double cogRipple = cogWave * sagCap * dragAmt * 0.0024;

        const double delSamps = std::clamp((0.001 + sagCap * dragAmt * 0.015 + cogRipple) * sr, 4.0, maxDelLimit);
        const double pitchL = dragL.readCubic(delSamps);
        const double pitchR = dragR.readCubic(delSamps * 1.018);

        // 3. Collapsing Supply Rail & 100 Hz Rectified Filter-Cap Ripple Bleed
        const double vRail = std::clamp(1.0 - (starve * 0.54) - (sagCap * 0.38), 0.08, 1.0);
        humPh = fract(humPh + (100.0 / sr));
        const double psrrRipple = (std::abs(std::sin(humPh * kTwoPi)) - 0.636) * sagCap * starve * 0.028;
        const double biasShift  = (1.0 - vRail) * 0.42 + psrrRipple;

        // 4. Class-B Zero-Crossing Transistor Crossover Notch + Cold-Bias Gate
        const double deadzone = starve * (0.04 + sagCap * 0.14);
        const double dzDenom  = std::max(deadzone, 0.005);
        const double xNotchL  = pitchL - deadzone * 0.55 * std::tanh(pitchL / dzDenom);
        const double xNotchR  = pitchR - deadzone * 0.55 * std::tanh(pitchR / dzDenom);

        const double sigLevel = std::max(std::abs(pitchL), std::abs(pitchR));
        const double gateTrig = (sigLevel > deadzone * 0.75) ? 1.0 : 0.0;
        const double gateRate = (gateTrig > gateEnv) ? 0.065 : 0.0032;
        gateEnv += (gateTrig - gateEnv) * gateRate;

        // 5. Asymmetric Rail Ceiling Collapse
        const double biasedL = xNotchL + biasShift;
        const double biasedR = xNotchR + biasShift;
        const double biasTanh = std::tanh(biasShift / vRail);
        const double crushL  = (std::tanh(biasedL / vRail) - biasTanh) * vRail;
        const double crushR  = (std::tanh(biasedR / vRail) - biasTanh) * vRail;

        // 6. Op-Amp Tail-Current Slew-Rate Starvation
        const double maxSlew = std::clamp(vRail * vRail * 0.45, 0.008, 1.0);
        slewL += std::clamp(crushL - slewL, -maxSlew, maxSlew);
        slewR += std::clamp(crushR - slewR, -maxSlew, maxSlew);

        const double gateMix  = mix(1.0, gateEnv, starve * 0.85);
        const double sputterL = slewL * gateMix;
        const double sputterR = slewR * gateMix;

        // 7. Phase-Transparent 4.5 Hz DC Blocker
        dcYL = sputterL - dcXL + rDc * dcYL;
        dcXL = sputterL;
        dcYR = sputterR - dcXR + rDc * dcYR;
        dcXR = sputterR;

        leftChannel[i]  = static_cast<float>(mix(inL, dcYL * makeup, wetMix));
        rightChannel[i] = static_cast<float>(mix(inR, dcYR * makeup, wetMix));
    }
}

juce::AudioProcessorEditor* VoltageStarveAudioProcessor::createEditor()
{
    return new VoltageStarveAudioProcessorEditor(*this);
}

void VoltageStarveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VoltageStarveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoltageStarveAudioProcessor();
}