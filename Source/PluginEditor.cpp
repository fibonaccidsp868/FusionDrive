#include "PluginEditor.h"

void FusionLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float d = (float) juce::jmin (w, h) - 8.f, r = d / 2.f, cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const bool on = s.isEnabled();
    juce::Path track; track.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff2a2d33)); g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (cx, cy, r, r, 0, a0, ang, true);
    g.setColour (on ? accent : juce::Colour (0xff555a63)); g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float kr = r - 8.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4e57), cx, cy - kr, juce::Colour (0xff1c1e22), cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (cx - kr, cy - kr, kr * 2, kr * 2, 1.f);
    juce::Path tick; tick.addRoundedRectangle (-1.8f, -kr + 2.f, 3.6f, kr * 0.45f, 1.5f);
    g.setColour (on ? juce::Colours::white : juce::Colours::grey);
    g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void FusionLNF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.18f) : juce::Colour (0xff23262b)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (on ? accent : juce::Colour (hover ? 0xff7a808a : 0xff4a4f58)); g.drawRoundedRectangle (r, 6.f, 1.5f);
    g.setColour (on ? accent : juce::Colours::grey); g.fillEllipse (r.getX() + 10, r.getCentreY() - 4, 8, 8);
    g.setColour (on ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (24).toNearestInt(), juce::Justification::centredLeft);
}

void FusionEditor::addKnob (Knob& k, const char* id, const char* text)
{
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
    k.s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.s.setLookAndFeel (&lnf); k.s.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    k.l.setText (text, juce::dontSendNotification); k.l.setJustificationType (juce::Justification::centred);
    k.l.setFont (juce::FontOptions (12.f, juce::Font::bold)); k.l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (k.s); addAndMakeVisible (k.l);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
}

FusionEditor::FusionEditor (FusionProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (880, 580);
    addKnob (drive, "drive", "DRIVE"); addKnob (tone, "tone", "TONE"); addKnob (character, "char", "CHARACTER"); addKnob (knee, "knee", "HARDNESS");
    addKnob (iron, "iron", "IRON"); addKnob (warmth, "warmth", "WARMTH"); addKnob (sag, "sag", "SAG"); addKnob (slew, "slew", "SLEW"); addKnob (drift, "drift", "DRIFT");
    addKnob (inTrim, "in", "INPUT"); addKnob (outTrim, "out", "OUTPUT"); addKnob (mix, "mix", "MIX");
    for (auto* b : { &analogBtn, &autoBtn, &msBtn }) { b->setLookAndFeel (&lnf); addAndMakeVisible (b); }
    aAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "analog", analogBtn);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "auto", autoBtn);
    msAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "ms", msBtn);
    osBox.addItemList ({ "Off", "2x", "4x", "8x" }, 1); addAndMakeVisible (osBox);
    osAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", osBox);
    osLabel.setText ("OVERSAMPLING", juce::dontSendNotification); osLabel.setFont (juce::FontOptions (11.f, juce::Font::bold));
    osLabel.setColour (juce::Label::textColourId, juce::Colours::grey); addAndMakeVisible (osLabel);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    analogBtn.onStateChange = [this] { updateAnalogState(); };
    updateAnalogState();
    startTimerHz (30);
}

FusionEditor::~FusionEditor()
{
    for (auto* k : { &drive, &tone, &character, &knee, &iron, &warmth, &sag, &slew, &drift, &inTrim, &outTrim, &mix }) k->s.setLookAndFeel (nullptr);
    for (auto* b : { &analogBtn, &autoBtn, &msBtn }) b->setLookAndFeel (nullptr);
}

void FusionEditor::updateAnalogState()
{
    const bool a = analogBtn.getToggleState();
    lnf.accent = a ? juce::Colour (0xff3DD6C6) : juce::Colour (0xffE8A33D);
    for (auto* k : { &iron, &warmth, &sag, &slew, &drift }) { k->s.setEnabled (a); k->l.setEnabled (a); k->s.setAlpha (a ? 1.f : 0.4f); k->l.setAlpha (a ? 1.f : 0.4f); }
    for (auto* k : { &tone, &character, &knee }) { k->s.setAlpha (1.f); }
    repaint();
}

void FusionEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (44);
    presetBox.setBounds (top.removeFromRight (230).reduced (0, 8));
    top.removeFromLeft (300);
    analogBtn.setBounds (top.removeFromLeft (140).reduced (0, 4));
    r.removeFromTop (8);
    specArea = r.removeFromTop (150).withTrimmedRight (52);
    r.removeFromTop (10);
    auto place = [] (juce::Rectangle<int> area, std::initializer_list<Knob*> ks)
    {
        const int w = area.getWidth() / (int) ks.size(); int i = 0;
        for (auto* k : ks) { auto c = area.withX (area.getX() + i++ * w).withWidth (w); k->l.setBounds (c.removeFromTop (16)); k->s.setBounds (c); }
    };
    auto row1 = r.removeFromTop (150); place (row1, { &drive, &tone, &character, &knee });
    r.removeFromTop (6);
    auto row2 = r.removeFromTop (120); place (row2, { &iron, &warmth, &sag, &slew, &drift });
    r.removeFromTop (6);
    auto bot = r.removeFromTop (110);
    auto btns = bot.removeFromRight (180);
    autoBtn.setBounds (btns.removeFromTop (34).reduced (0, 2)); msBtn.setBounds (btns.removeFromTop (34).reduced (0, 2));
    osLabel.setBounds (btns.removeFromTop (16)); osBox.setBounds (btns.removeFromTop (24));
    place (bot, { &inTrim, &outTrim, &mix });
}

void FusionEditor::timerCallback()
{
    const int avail = proc.fifo.getNumReady();
    if (avail > 0)
    {
        std::vector<float> tmp ((size_t) avail);
        int s1, z1, s2, z2; proc.fifo.prepareToRead (avail, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) tmp[(size_t) i] = proc.fifoData[(size_t) (s1 + i)];
        for (int i = 0; i < z2; ++i) tmp[(size_t) (z1 + i)] = proc.fifoData[(size_t) (s2 + i)];
        proc.fifo.finishedRead (z1 + z2);
        const int n = juce::jmin (avail, 2048);
        std::move (timeBuf.begin() + n, timeBuf.end(), timeBuf.begin());
        std::copy (tmp.end() - n, tmp.end(), timeBuf.end() - n);
    }
    std::copy (timeBuf.begin(), timeBuf.end(), fftBuf.begin()); std::fill (fftBuf.begin() + 2048, fftBuf.end(), 0.f);
    window.multiplyWithWindowingTable (fftBuf.data(), 2048);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());
    const double sr = juce::jmax (8000.0, proc.getSampleRate());
    for (size_t i = 0; i < spec.size(); ++i)
    {
        const double fr = 20.0 * std::pow (1000.0, (double) i / (double) (spec.size() - 1));
        const int bin = juce::jlimit (1, 1023, (int) std::round (fr / sr * 2048.0));
        const float db = juce::Decibels::gainToDecibels (fftBuf[(size_t) bin] / 512.f, -120.f);
        spec[i] += 0.4f * (db - spec[i]); peak[i] = juce::jmax (peak[i] - 0.4f, spec[i]);
    }
    inMeter = juce::jmax (proc.inLevel.load(), inMeter * 0.9f); outMeter = juce::jmax (proc.outLevel.load(), outMeter * 0.9f);
    repaint (specArea.expanded (40, 4));
}

void FusionEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171a));
    g.setColour (lnf.accent); g.setFont (juce::FontOptions (26.f, juce::Font::bold));
    g.drawText ("FUSION DRIVE", 16, 8, 300, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey); g.setFont (juce::FontOptions (11.f));
    g.drawText (analogBtn.getToggleState() ? "ANALOG ENGINE  -  transformer / class-A / slew" : "VINTAGE ENGINE  -  asymmetric drive", 16, 38, 400, 14, juce::Justification::centredLeft);

    auto a = specArea.toFloat();
    g.setColour (juce::Colour (0xff0e0f11)); g.fillRoundedRectangle (a, 6.f);
    g.setColour (juce::Colour (0xff23262b));
    for (int db = -100; db <= -20; db += 20) { const float yy = a.getBottom() - (float) (db + 110) / 110.f * a.getHeight(); g.drawHorizontalLine ((int) yy, a.getX(), a.getRight()); }
    for (double f : { 100.0, 1000.0, 10000.0 }) { const float xx = a.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * a.getWidth(); g.drawVerticalLine ((int) xx, a.getY(), a.getBottom()); }
    auto mk = [&] (const std::array<float, 1024>& d, bool fill)
    {
        juce::Path p; p.startNewSubPath (a.getX(), a.getBottom());
        for (size_t i = 0; i < d.size(); ++i)
            p.lineTo (a.getX() + (float) i / (float) (d.size() - 1) * a.getWidth(), a.getBottom() - juce::jlimit (0.f, 1.f, (d[i] + 110.f) / 110.f) * a.getHeight());
        if (fill) { p.lineTo (a.getRight(), a.getBottom()); p.closeSubPath(); g.setColour (lnf.accent.withAlpha (0.22f)); g.fillPath (p); }
        else { g.setColour (lnf.accent.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.f)); }
    };
    // note: spec has 1024 entries but array<float,1024> alias for both buffers
    mk (peak, false); mk (spec, true);
    g.setColour (juce::Colours::grey); g.setFont (juce::FontOptions (10.f));
    g.drawText ("100", (int) (a.getX() + std::log (5.0) / std::log (1000.0) * a.getWidth()) + 2, (int) a.getBottom() - 14, 40, 12, juce::Justification::left);
    g.drawText ("1k", (int) (a.getX() + std::log (50.0) / std::log (1000.0) * a.getWidth()) + 2, (int) a.getBottom() - 14, 40, 12, juce::Justification::left);
    g.drawText ("10k", (int) (a.getX() + std::log (500.0) / std::log (1000.0) * a.getWidth()) + 2, (int) a.getBottom() - 14, 40, 12, juce::Justification::left);
    // meters
    auto meter = [&] (float x, float lvl, const char* t)
    {
        g.setColour (juce::Colour (0xff23262b)); g.fillRect (x, a.getY(), 8.f, a.getHeight());
        const float h = juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (lvl, -60.f) + 60.f) / 66.f) * a.getHeight();
        g.setColour (lvl > 1.f ? juce::Colours::red : lnf.accent); g.fillRect (x, a.getBottom() - h, 8.f, h);
        g.setColour (juce::Colours::grey); g.setFont (juce::FontOptions (9.f)); g.drawText (t, (int) x - 6, (int) a.getBottom() + 1, 20, 10, juce::Justification::centred);
    };
    meter (a.getRight() + 10, inMeter, "IN"); meter (a.getRight() + 26, outMeter, "OUT");
}
