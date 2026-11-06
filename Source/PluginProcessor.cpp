#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset { const char* name; float drive, tone, character, knee; bool analog; float iron, warmth, sag, slew, drift, mix; };
const Preset presets[] = {
    { "Init (Vintage)",       3.f,   0.f,  30.f, 35.f, false, 50.f, 50.f, 30.f, 30.f, 30.f, 100.f },
    { "Bus Glue",             2.5f, 10.f,  20.f, 25.f, false, 50.f, 50.f, 30.f, 30.f, 30.f, 100.f },
    { "Crunchy Drums",        6.5f, 25.f,  40.f, 55.f, false, 50.f, 50.f, 30.f, 30.f, 30.f,  70.f },
    { "Warm Master (Analog)", 3.f,  -10.f, 35.f, 35.f, true,  45.f, 65.f, 25.f, 20.f, 25.f, 100.f },
    { "Iron Heavy (Analog)",  5.5f, -25.f, 50.f, 40.f, true,  85.f, 70.f, 55.f, 35.f, 35.f, 100.f },
    { "Vocal Silk (Analog)",  2.f,   15.f, 45.f, 30.f, true,  35.f, 55.f, 20.f, 15.f, 20.f,  80.f },
    { "Parallel Grit",        8.f,   30.f, 60.f, 65.f, true,  70.f, 40.f, 60.f, 55.f, 40.f,  35.f },
};
}

juce::AudioProcessorValueTreeState::ParameterLayout FusionProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> txt = nullptr)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def,
                     AudioParameterFloatAttributes().withStringFromValueFunction (txt)));
    };
    f ("drive",  "Drive",     0.f, 10.f, 3.f,  [] (float v, int) { return String (v, 1); });
    f ("tone",   "Tone",     -100.f, 100.f, 0.f, pct);
    f ("char",   "Character", 0.f, 100.f, 30.f, pct);
    f ("knee",   "Hardness",  0.f, 100.f, 35.f, pct);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "analog", 1 }, "Analog", false));
    f ("iron",   "Iron",      0.f, 100.f, 50.f, pct);
    f ("warmth", "Warmth",    0.f, 100.f, 50.f, pct);
    f ("sag",    "Sag",       0.f, 100.f, 30.f, pct);
    f ("slew",   "Slew",      0.f, 100.f, 30.f, pct);
    f ("drift",  "Drift",     0.f, 100.f, 30.f, pct);
    f ("in",     "Input",    -24.f, 24.f, 0.f, [] (float v, int) { return String (v, 1) + " dB"; });
    f ("out",    "Output",   -24.f, 24.f, 0.f, [] (float v, int) { return String (v, 1) + " dB"; });
    f ("mix",    "Mix",       0.f, 100.f, 100.f, pct);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "os", 1 }, "Oversampling", StringArray { "Off", "2x", "4x", "8x" }, 2));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "auto", 1 }, "Auto Gain", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ms", 1 }, "Mid/Side", false));
    return { p.begin(), p.end() };
}

FusionProcessor::FusionProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pDrive = g ("drive"); pTone = g ("tone"); pChar = g ("char"); pKnee = g ("knee"); pAnalog = g ("analog");
    pIron = g ("iron"); pWarmth = g ("warmth"); pSag = g ("sag"); pSlew = g ("slew"); pDrift = g ("drift");
    pIn = g ("in"); pOut = g ("out"); pMix = g ("mix"); pOs = g ("os"); pAuto = g ("auto"); pMS = g ("ms");
}

bool FusionProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void FusionProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = samplesPerBlock;
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, i + 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
        oversamplers[(size_t) i]->initProcessing ((size_t) samplesPerBlock);
    }
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    dryDelay.prepare (spec);
    dryBuffer.setSize (2, samplesPerBlock);
    osIndex = -1;
    configureOversampling ((int) pOs->load());
    agIn = agOut = 1.0e-4f; agGain = 1.0f;
}

void FusionProcessor::configureOversampling (int index)
{
    osIndex = index;
    const int factor = 1 << index;
    for (size_t c = 0; c < 2; ++c) chans[c].prepare (sr * factor, (int) c);
    int latency = 0;
    if (index > 0) { oversamplers[(size_t) index - 1]->reset(); latency = (int) std::ceil (oversamplers[(size_t) index - 1]->getLatencyInSamples()); }
    dryDelay.reset(); dryDelay.setDelay ((float) latency);
    setLatencySamples (latency);
}

void FusionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2), n = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0 || n > dryBuffer.getNumSamples()) return;

    if ((int) pOs->load() != osIndex) configureOversampling ((int) pOs->load());
    const int factor = 1 << osIndex;

    fd::Params prm;
    prm.driveDb = pDrive->load() * 3.0; prm.tone = pTone->load() / 100.0; prm.character = pChar->load() / 100.0;
    prm.knee = pKnee->load() / 100.0; prm.analog = pAnalog->load() > 0.5f; prm.iron = pIron->load() / 100.0;
    prm.warmth = pWarmth->load() / 100.0; prm.sag = pSag->load() / 100.0; prm.slew = pSlew->load() / 100.0;
    prm.drift = pDrift->load() / 100.0;
    for (int c = 0; c < 2; ++c) chans[(size_t) c].setParams (prm);

    // dry path (latency matched)
    for (int c = 0; c < numCh; ++c) dryBuffer.copyFrom (c, 0, buffer, c, 0, n);
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = dryBuffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) { dryDelay.pushSample (c, d[i]); d[i] = dryDelay.popSample (c); }
    }

    const float inG = juce::Decibels::decibelsToGain (pIn->load());
    buffer.applyGain (inG);
    for (int c = 0; c < numCh; ++c) for (int i = 0; i < n; ++i) dryBuffer.getWritePointer (c)[i] *= inG;

    float peakIn = 0.f;
    for (int c = 0; c < numCh; ++c) peakIn = juce::jmax (peakIn, buffer.getMagnitude (c, 0, n));
    inLevel.store (peakIn);

    const bool ms = pMS->load() > 0.5f && numCh == 2;
    if (ms) for (int i = 0; i < n; ++i)
    {
        const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
        buffer.setSample (0, i, 0.5f * (l + r)); buffer.setSample (1, i, 0.5f * (l - r));
    }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto run = [this, numCh] (juce::dsp::AudioBlock<float>& b)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = b.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < b.getNumSamples(); ++i) d[i] = (float) chans[(size_t) c].process ((double) d[i]);
        }
    };
    if (osIndex == 0) run (block);
    else
    {
        auto& os = *oversamplers[(size_t) osIndex - 1];
        auto up = os.processSamplesUp (block);
        run (up);
        os.processSamplesDown (block);
    }

    if (ms) for (int i = 0; i < n; ++i)
    {
        const float m = buffer.getSample (0, i), s = buffer.getSample (1, i);
        buffer.setSample (0, i, m + s); buffer.setSample (1, i, m - s);
    }

    // auto gain: slow RMS match between dry and wet
    if (pAuto->load() > 0.5f)
    {
        const float k = 1.0f - std::exp (-(float) n / (0.4f * (float) sr));
        float ei = 0.f, eo = 0.f;
        for (int c = 0; c < numCh; ++c)
        {
            const auto* d = dryBuffer.getReadPointer (c); const auto* w = buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i) { ei += d[i] * d[i]; eo += w[i] * w[i]; }
        }
        ei /= (float) (n * numCh); eo /= (float) (n * numCh);
        agIn += k * (ei - agIn); agOut += k * (eo - agOut);
        const float target = juce::jlimit (0.125f, 4.0f, std::sqrt ((agIn + 1.0e-9f) / (agOut + 1.0e-9f)));
        const float g0 = agGain; agGain += k * (target - agGain);
        for (int c = 0; c < numCh; ++c) buffer.applyGainRamp (c, 0, n, g0, agGain);
    }
    else agGain = 1.0f;

    const float outG = juce::Decibels::decibelsToGain (pOut->load()), mix = pMix->load() / 100.0f;
    for (int c = 0; c < numCh; ++c)
    {
        auto* w = buffer.getWritePointer (c); const auto* d = dryBuffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) w[i] = (w[i] * mix + d[i] * (1.0f - mix)) * outG;
    }

    float peakOut = 0.f;
    for (int c = 0; c < numCh; ++c) peakOut = juce::jmax (peakOut, buffer.getMagnitude (c, 0, n));
    outLevel.store (peakOut);

    // spectrum feed (mono sum)
    int s1, z1, s2, z2; fifo.prepareToWrite (n, s1, z1, s2, z2);
    auto push = [&] (int start, int size, int offset)
    {
        for (int i = 0; i < size; ++i)
        {
            float v = 0.f; for (int c = 0; c < numCh; ++c) v += buffer.getSample (c, offset + i);
            fifoData[(size_t) (start + i)] = v / (float) numCh;
        }
    };
    push (s1, z1, 0); if (z2 > 0) push (s2, z2, z1);
    fifo.finishedWrite (z1 + z2);
}

int FusionProcessor::getNumPrograms() { return (int) (sizeof (presets) / sizeof (presets[0])); }
const juce::String FusionProcessor::getProgramName (int i) { return presets[juce::jlimit (0, getNumPrograms() - 1, i)].name; }

void FusionProcessor::setCurrentProgram (int i)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, i);
    const auto& p = presets[currentProgram];
    auto set = [this] (const char* id, float v) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set ("drive", p.drive); set ("tone", p.tone); set ("char", p.character); set ("knee", p.knee); set ("analog", p.analog ? 1.f : 0.f);
    set ("iron", p.iron); set ("warmth", p.warmth); set ("sag", p.sag); set ("slew", p.slew); set ("drift", p.drift); set ("mix", p.mix);
}

void FusionProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void FusionProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FusionProcessor::createEditor() { return new FusionEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FusionProcessor(); }
