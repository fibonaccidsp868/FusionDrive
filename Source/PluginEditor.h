#pragma once
#include "PluginProcessor.h"

class FusionLNF : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { 0xffE8A33D };
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class FusionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FusionEditor (FusionProcessor&);
    ~FusionEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Knob
    {
        juce::Slider s; juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void timerCallback() override;
    void addKnob (Knob&, const char* id, const char* text);
    void updateAnalogState();

    FusionProcessor& proc;
    FusionLNF lnf;
    Knob drive, tone, character, knee, iron, warmth, sag, slew, drift, inTrim, outTrim, mix;
    juce::ToggleButton analogBtn { "ANALOG" }, autoBtn { "AUTO GAIN" }, msBtn { "MID/SIDE" };
    juce::ComboBox osBox, presetBox; juce::Label osLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aAtt, autoAtt, msAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAtt;

    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2048> timeBuf {}; std::array<float, 4096> fftBuf {}; std::array<float, 1024> spec {}, peak {};
    float inMeter = 0, outMeter = 0;
    juce::Rectangle<int> specArea;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FusionEditor)
};
