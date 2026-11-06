#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "FusionDsp.h"

class FusionProcessor : public juce::AudioProcessor
{
public:
    FusionProcessor();
    ~FusionProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FusionDrive"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // GUI feeds
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };
    static constexpr int fifoSize = 16384;
    juce::AbstractFifo fifo { fifoSize };
    std::array<float, fifoSize> fifoData {};

private:
    void configureOversampling (int index);

    std::array<fd::Channel, 2> chans;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers; // 2x,4x,8x
    int osIndex = -1;                       // -1 = not configured, 0 = off, 1..3 = 2x..8x
    int currentProgram = 0;
    double sr = 44100.0;
    int maxBlock = 512;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer;
    float agIn = 1.0e-4f, agOut = 1.0e-4f, agGain = 1.0f;

    std::atomic<float>* pDrive; std::atomic<float>* pTone; std::atomic<float>* pChar; std::atomic<float>* pKnee;
    std::atomic<float>* pAnalog; std::atomic<float>* pIron; std::atomic<float>* pWarmth; std::atomic<float>* pSag;
    std::atomic<float>* pSlew; std::atomic<float>* pDrift; std::atomic<float>* pIn; std::atomic<float>* pOut;
    std::atomic<float>* pMix; std::atomic<float>* pOs; std::atomic<float>* pAuto; std::atomic<float>* pMS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FusionProcessor)
};
