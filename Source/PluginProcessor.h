#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>
#include <algorithm>

class CubicDelayLine
{
public:
    void prepare(int maxSamples)
    {
        size = std::max(maxSamples, 44100) + 8;
        buffer.assign(static_cast<size_t>(size), 0.0);
        writeIndex = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0);
        writeIndex = 0;
    }

    inline void write(double sample)
    {
        buffer[static_cast<size_t>(writeIndex)] = sample;
        if (++writeIndex >= size)
            writeIndex = 0;
    }

    inline double readCubic(double delaySamples) const
    {
        double readPos = static_cast<double>(writeIndex - 1) - delaySamples;
        while (readPos < 0.0)
            readPos += static_cast<double>(size);

        int idx1 = static_cast<int>(readPos) % size;
        double frac = readPos - std::floor(readPos);

        int idx0 = (idx1 - 1 + size) % size;
        int idx2 = (idx1 + 1) % size;
        int idx3 = (idx1 + 2) % size;

        double y0 = buffer[static_cast<size_t>(idx0)];
        double y1 = buffer[static_cast<size_t>(idx1)];
        double y2 = buffer[static_cast<size_t>(idx2)];
        double y3 = buffer[static_cast<size_t>(idx3)];

        // 4-point Catmull-Rom cubic interpolation
        double c0 = y1;
        double c1 = 0.5 * (y2 - y0);
        double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

private:
    std::vector<double> buffer;
    int size = 44108;
    int writeIndex = 0;
};

class VoltageStarveAudioProcessor : public juce::AudioProcessor
{
public:
    VoltageStarveAudioProcessor();
    ~VoltageStarveAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Delay lines
    CubicDelayLine dragL, dragR;

    // Gen~ History states
    double sagCap  = 0.0;
    double cogPh   = 0.0;
    double humPh   = 0.0;
    double gateEnv = 1.0;
    double slewL   = 0.0;
    double slewR   = 0.0;
    double dcXL    = 0.0;
    double dcYL    = 0.0;
    double dcXR    = 0.0;
    double dcYR    = 0.0;

    double currentSampleRate = 44100.0;

    // Parameter pointers
    std::atomic<float>* starveParam = nullptr;
    std::atomic<float>* dragParam   = nullptr;
    std::atomic<float>* envParam    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoltageStarveAudioProcessor)
};