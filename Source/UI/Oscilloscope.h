#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "UI/Scope.h"
#include "UI/Theme.h"

namespace xum
{
// Osciloscopio en tiempo real: trazo neón con halo, rejilla tenue, disparo por flanco.
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope (ScopeBuffer& s) : scope (s) { data.fill (0.f); startTimerHz (30); }
    ~Oscilloscope() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f);
        g.setColour (Theme::darkPanel);  g.fillRoundedRectangle (r, 6.f);
        g.setColour (Theme::line);   g.drawRoundedRectangle (r, 6.f, 1.f);

        g.setColour (Theme::line.withAlpha (0.7f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.f), r.getY(), r.getBottom());

        juce::Path p;
        const float h = r.getHeight() * 0.42f;
        for (int i = 0; i < (int) data.size(); ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (data.size() - 1);
            const float y = r.getCentreY() - juce::jlimit (-1.f, 1.f, data[(size_t) i]) * h;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (Theme::accent.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (9.f));
        g.setColour (Theme::accent.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (4.f));
        g.setColour (Theme::accent);                   g.strokePath (p, juce::PathStrokeType (1.4f));
    }

private:
    void timerCallback() override
    {
        scope.readTriggered (data.data(), (int) data.size());
        float peak = 0.f;
        for (float v : data) peak = std::max (peak, std::abs (v));
        const bool silent = peak < 1e-4f;
        if (silent && wasSilent) return;            // sin audio: no se repinta (ahorra CPU)
        wasSilent = silent;
        repaint();
    }

    bool wasSilent = false;

    ScopeBuffer& scope;
    std::array<float, 1024> data;
};
}
