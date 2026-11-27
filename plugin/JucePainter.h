// SPDX-License-Identifier: AGPL-3.0-or-later
// Draws the ELEKTAKT interface with juce::Graphics (design coordinates 1600 × 900).
#pragma once
#include <JuceHeader.h>
#include "ui.hpp"

class JucePainter : public trig::Painter {
public:
    explicit JucePainter(juce::Graphics& graphics) : g(graphics) {}
    static juce::Colour col(trig::Color c) { return juce::Colour(c.r, c.g, c.b); }
    void fill(int x, int y, int w, int h, trig::Color c) override {
        g.setColour(col(c));
        g.fillRect(x, y, w, h);
    }
    void frame(int x, int y, int w, int h, trig::Color c, int t) override {
        g.setColour(col(c));
        g.drawRect(x, y, w, h, t);
    }
    void round(int x, int y, int w, int h, int r, trig::Color c) override {
        g.setColour(col(c));
        g.fillRoundedRectangle(float(x), float(y), float(w), float(h), float(r));
    }
    void roundFrame(int x, int y, int w, int h, int r, trig::Color c, int t) override {
        g.setColour(col(c));
        g.drawRoundedRectangle(float(x) + t * .5f, float(y) + t * .5f, float(w - t), float(h - t), float(r), float(t));
    }
    void text(int x, int y, int w, int h, const std::string& s, int size, trig::Color c, trig::Align a, bool bold) override {
        g.setColour(col(c));
        g.setFont(juce::Font(juce::FontOptions(float(size) * 1.08f, bold ? juce::Font::bold : juce::Font::plain)));
        g.drawText(juce::String::fromUTF8(s.c_str()), x, y, w, h,
                   a == trig::Align::Left     ? juce::Justification::centredLeft
                   : a == trig::Align::Center ? juce::Justification::centred
                                              : juce::Justification::centredRight,
                   true);
    }
    void line(int x1, int y1, int x2, int y2, trig::Color c, int w) override {
        g.setColour(col(c));
        g.drawLine(float(x1), float(y1), float(x2), float(y2), float(w));
    }
    void arc(int cx, int cy, int r, double start, double sweep, trig::Color c, int w) override {
        juce::Path path;
        const float a0 = float((start + 90.) * juce::MathConstants<double>::pi / 180.),
                    a1 = float((start + sweep + 90.) * juce::MathConstants<double>::pi / 180.);
        path.addCentredArc(float(cx), float(cy), float(r), float(r), 0.f, a0, a1, true);
        g.setColour(col(c));
        g.strokePath(path, juce::PathStrokeType(float(w), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void disc(int cx, int cy, int r, trig::Color c) override {
        g.setColour(col(c));
        g.fillEllipse(float(cx - r), float(cy - r), float(2 * r), float(2 * r));
    }

private:
    juce::Graphics& g;
};
