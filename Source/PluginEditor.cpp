#include "PluginEditor.h"
#include "Log.h"
#include <typeinfo>

using namespace juce;
using namespace steinex;

//==============================================================================
namespace
{
namespace col
{
    const Colour bg0 (0xff06080c), bg1 (0xff0c1016), bg2 (0xff131a26), bg3 (0xff1a2333), stroke (0xff252e42),
                 text (0xffe9eef7), dim (0xff7b879d), cyan (0xff35d7ff), violet (0xff8a63ff),
                 magenta (0xffff3d8b), red (0xffff3b4e), amber (0xffffb547), green (0xff3de8a0);
}

Font fnt (float h, bool bold = false)
{
   #if JUCE_WINDOWS
    return Font (FontOptions ("Segoe UI", h, bold ? Font::bold : Font::plain));
   #else
    return Font (FontOptions (Font::getDefaultSansSerifFontName(), h, bold ? Font::bold : Font::plain));
   #endif
}

void drawCard (Graphics& g, Rectangle<float> r, float radius = 12.f)
{
    g.setGradientFill (ColourGradient (col::bg2.withAlpha (0.95f), r.getX(), r.getY(), col::bg1, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (col::stroke);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + radius, r.getY() + 1.f, r.getRight() - radius, r.getY() + 1.f);
}

void drawLogoMark (Graphics& g, Rectangle<float> r)
{
    g.setColour (col::cyan.withAlpha (0.18f));
    g.fillRoundedRectangle (r.expanded (4.f), r.getWidth() * 0.32f);
    g.setGradientFill (ColourGradient (col::cyan, r.getX(), r.getY(), col::violet, r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, r.getWidth() * 0.26f);
    g.setColour (col::bg0.withAlpha (0.9f));
    const auto c = r.getCentre();
    const float s = r.getWidth() * 0.2f;
    for (int k = 0; k < 2; ++k)
    {
        Path p;
        const float ox = c.x + ((float) k - 0.5f) * s * 1.4f - s * 0.1f;
        p.startNewSubPath (ox - s * 0.55f, c.y - s);
        p.lineTo (ox + s * 0.45f, c.y);
        p.lineTo (ox - s * 0.55f, c.y + s);
        g.strokePath (p, PathStrokeType (r.getWidth() * 0.095f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void drawSpaced (Graphics& g, const String& text, float x, float y, float h, float spacing, bool bold, float fontH)
{
    const Font f = fnt (fontH, bold);
    g.setFont (f);
    for (auto c : text)
    {
        const String ch = String::charToString (c);
        const float w = f.getStringWidthFloat (ch);
        g.drawText (ch, Rectangle<float> (x, y, w + 4.f, h), Justification::centredLeft);
        x += w + spacing;
    }
}

String formatTime (double seconds)
{
    const int m = (int) (seconds / 60.0);
    const double s = seconds - m * 60.0;
    return String (m).paddedLeft ('0', 2) + ":" + String (s, 1).paddedLeft ('0', 4);
}

//==============================================================================
class SteinexLnF : public LookAndFeel_V4
{
public:
    SteinexLnF()
    {
        setColour (ComboBox::backgroundColourId, col::bg2);
        setColour (ComboBox::outlineColourId, col::stroke);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::arrowColourId, col::dim);
        setColour (PopupMenu::backgroundColourId, col::bg1);
        setColour (PopupMenu::textColourId, col::text);
        setColour (PopupMenu::highlightedBackgroundColourId, col::cyan.withAlpha (0.22f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (AlertWindow::backgroundColourId, col::bg1);
        setColour (AlertWindow::textColourId, col::text);
        setColour (AlertWindow::outlineColourId, col::stroke);
        setColour (TextEditor::backgroundColourId, col::bg2);
        setColour (TextEditor::textColourId, col::text);
        setColour (TextEditor::outlineColourId, col::stroke);
        setColour (TextEditor::focusedOutlineColourId, col::cyan);
        setColour (TextButton::buttonColourId, col::bg3);
        setColour (TextButton::textColourOffId, col::text);
        setColour (TextButton::textColourOnId, col::text);
    }

    Font getComboBoxFont (ComboBox&) override      { return fnt (13.f); }
    Font getPopupMenuFont() override               { return fnt (14.f); }
    Font getTextButtonFont (TextButton&, int) override { return fnt (12.f, true); }
    Font getLabelFont (Label&) override            { return fnt (12.f); }
    Font getAlertWindowMessageFont() override      { return fnt (14.f); }
    Font getAlertWindowTitleFont() override        { return fnt (17.f, true); }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s) override
    {
        STX_ONCE();
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
        const float d = jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;
        const Colour acc = s.findColour (Slider::rotarySliderFillColourId);
        const bool hot = s.isMouseOverOrDragging();

        // body
        const float br = r * 0.74f;
        g.setGradientFill (ColourGradient (Colour (0xff2a3347), c.x - br * 0.5f, c.y - br * 0.7f,
                                           Colour (0xff0a0d13), c.x + br * 0.6f, c.y + br * 0.9f, true));
        g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour (Colours::white.withAlpha (hot ? 0.16f : 0.08f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.f);

        // track
        const float ar = r * 0.93f;
        Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
        g.setColour (Colour (0xff222b3c));
        g.strokePath (track, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));

        const float ang = startAngle + pos * (endAngle - startAngle);
        if (pos > 0.002f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, ang, true);
            g.setColour (acc.withAlpha (0.22f));
            g.strokePath (val, PathStrokeType (8.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (acc);
            g.strokePath (val, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // pointer
        const auto p1 = c.getPointOnCircumference (br * 0.30f, ang);
        const auto p2 = c.getPointOnCircumference (br * 0.86f, ang);
        g.setColour (Colours::white);
        g.drawLine (Line<float> (p1, p2), 2.2f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           Slider::SliderStyle, Slider& s) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        const Colour acc = s.findColour (Slider::trackColourId);
        g.setColour (Colour (0xff222b3c));
        g.fillRoundedRectangle ((float) x, cy - 2.f, (float) w, 4.f, 2.f);
        g.setColour (acc);
        g.fillRoundedRectangle ((float) x, cy - 2.f, sliderPos - (float) x, 4.f, 2.f);
        g.setColour (Colours::white);
        g.fillEllipse (sliderPos - 6.f, cy - 6.f, 12.f, 12.f);
    }

    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        STX_ONCE();
        auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::bg2);
        g.fillRoundedRectangle (r, 7.f);
        g.setColour (box.isPopupActive() ? col::cyan.withAlpha (0.8f) : col::stroke);
        g.drawRoundedRectangle (r, 7.f, 1.f);
        Path a;
        const float cx = (float) w - 15.f, cy = (float) h * 0.5f;
        a.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour (col::dim);
        g.fillPath (a);
    }

    void positionComboBoxText (ComboBox& box, Label& label) override
    {
        label.setBounds (9, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (Graphics& g, int w, int h) override
    {
        g.fillAll (col::bg1);
        g.setColour (col::stroke);
        g.drawRect (0, 0, w, h);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        Colour c = col::bg3;
        if (over) c = c.brighter (0.1f);
        if (down) c = c.darker (0.2f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 7.f);
        g.setColour (over ? col::cyan.withAlpha (0.6f) : col::stroke);
        g.drawRoundedRectangle (r, 7.f, 1.f);
    }
};

//==============================================================================
class KnobComp : public Component
{
public:
    KnobComp (AudioProcessorValueTreeState& apvts, const String& id, const String& name, Colour accent)
        : title (name.toUpperCase())
    {
        slider.setSliderStyle (Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (degreesToRadians (225.f), degreesToRadians (495.f), true);
        slider.setColour (Slider::rotarySliderFillColourId, accent);
        slider.setMouseDragSensitivity (200);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        acc = accent;
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromBottom (34);
        slider.setBounds (b);
    }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        auto b = getLocalBounds();
        auto lower = b.removeFromBottom (34);
        g.setColour (acc);
        g.setFont (fnt (12.5f, true));
        g.drawText (slider.getTextFromValue (slider.getValue()), lower.removeFromTop (18), Justification::centred);
        g.setColour (col::dim);
        g.setFont (fnt (10.f, true));
        g.drawText (title, lower, Justification::centredTop);
    }

private:
    String title;
    Colour acc;
    Slider slider;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

class ChoiceComp : public Component
{
public:
    ChoiceComp (AudioProcessorValueTreeState& apvts, const String& id, const String& name, const StringArray& items)
        : title (name.toUpperCase())
    {
        box.addItemList (items, 1);
        box.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (box);
        att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
    }
    void resized() override { auto b = getLocalBounds(); b.removeFromTop (17); box.setBounds (b.removeFromTop (28)); }
    void paint (Graphics& g) override
    {
        STX_ONCE();
        g.setColour (col::dim);
        g.setFont (fnt (10.f, true));
        g.drawText (title, getLocalBounds().removeFromTop (16), Justification::centredLeft);
    }
private:
    String title;
    ComboBox box;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class PillToggle : public ToggleButton
{
public:
    PillToggle (const String& text, Colour a) : ToggleButton (text), accent (a) {}
    void paintButton (Graphics& g, bool over, bool) override
    {
        STX_ONCE();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (on ? accent.withAlpha (0.22f) : col::bg2);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? accent : (over ? col::dim : col::stroke));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
        g.setColour (on ? Colours::white : col::dim);
        g.setFont (fnt (11.f, true));
        g.drawText (getButtonText().toUpperCase(), r, Justification::centred);
    }
private:
    Colour accent;
};

class PowerButton : public ToggleButton
{
public:
    explicit PowerButton (Colour a) : accent (a) {}
    void paintButton (Graphics& g, bool over, bool) override
    {
        STX_ONCE();
        auto r = getLocalBounds().toFloat().reduced (3.f);
        const bool on = getToggleState();
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.fillEllipse (r.expanded (3.f));
        }
        g.setColour (on ? accent.withAlpha (0.22f) : col::bg2);
        g.fillEllipse (r);
        g.setColour (on ? accent : (over ? col::dim : col::stroke));
        g.drawEllipse (r, 1.6f);

        const auto c = r.getCentre();
        const float rr = r.getWidth() * 0.26f;
        Path p;
        p.addCentredArc (c.x, c.y + 0.5f, rr, rr, 0.f, degreesToRadians (40.f), degreesToRadians (320.f), true);
        p.startNewSubPath (c.x, c.y - rr - 1.f);
        p.lineTo (c.x, c.y);
        g.setColour (on ? Colours::white : col::dim);
        g.strokePath (p, PathStrokeType (2.f, PathStrokeType::curved, PathStrokeType::rounded));
    }
private:
    Colour accent;
};

class SceneButton : public Button
{
public:
    SceneButton (const String& n, Colour c) : Button (n), accent (c) {}
    void paintButton (Graphics& g, bool over, bool down) override
    {
        STX_ONCE();
        auto r = getLocalBounds().toFloat().reduced (1.f);
        const bool on = getToggleState();
        Colour base = on ? accent.withAlpha (0.26f) : col::bg2;
        if (over) base = base.brighter (0.08f);
        if (down) base = base.darker (0.25f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (on ? accent : (over ? col::dim : col::stroke));
        g.drawRoundedRectangle (r, 8.f, on ? 1.6f : 1.f);
        g.setColour (accent.withAlpha (on ? 1.f : 0.6f));
        g.fillRoundedRectangle (r.getX() + 8.f, r.getCentreY() - 7.f, 3.f, 14.f, 1.5f);
        g.setColour (on ? Colours::white : col::text);
        g.setFont (fnt (11.5f, true));
        g.drawText (getButtonText(), r.withTrimmedLeft (20.f).withTrimmedRight (4.f), Justification::centredLeft);
    }
private:
    Colour accent;
};

class TransportButton : public Button
{
public:
    enum Kind { Rec, Stop, Play, Del };
    explicit TransportButton (Kind k) : Button (""), kind (k) {}
    bool pulsing = false, glyphStop = false;

    void paintButton (Graphics& g, bool over, bool down) override
    {
        STX_ONCE();
        auto r = getLocalBounds().toFloat().reduced (2.f);
        const bool en = isEnabled();
        const Colour c = kind == Rec ? col::red : kind == Play ? col::green : kind == Stop ? col::text : col::amber;

        g.setColour (down ? col::bg3 : col::bg2);
        g.fillEllipse (r);
        g.setColour (en ? c.withAlpha (over ? 1.f : 0.65f) : col::stroke);
        g.drawEllipse (r.reduced (0.5f), 1.6f);

        const auto ic = r.getCentre();
        const float s = r.getWidth() * 0.2f;
        float alpha = 1.f;
        if (pulsing) alpha = 0.55f + 0.45f * (float) std::sin ((double) Time::getMillisecondCounter() * 0.008);
        g.setColour (en ? c.withAlpha (alpha) : col::stroke);

        if (kind == Rec)
            g.fillEllipse (Rectangle<float> (s * 2.1f, s * 2.1f).withCentre (ic));
        else if (kind == Stop || (kind == Play && glyphStop))
            g.fillRoundedRectangle (Rectangle<float> (s * 1.9f, s * 1.9f).withCentre (ic), 2.f);
        else if (kind == Play)
        {
            Path p;
            p.addTriangle (ic.x - s * 0.75f, ic.y - s * 1.1f, ic.x - s * 0.75f, ic.y + s * 1.1f, ic.x + s * 1.2f, ic.y);
            g.fillPath (p);
        }
        else
        {
            Path p;
            p.startNewSubPath (ic.x - s, ic.y - s); p.lineTo (ic.x + s, ic.y + s);
            p.startNewSubPath (ic.x + s, ic.y - s); p.lineTo (ic.x - s, ic.y + s);
            g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
private:
    Kind kind;
};

//==============================================================================
class MeterView : public Component, private Timer
{
public:
    explicit MeterView (TransitFxProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (Graphics& g) override
    {
        STX_ONCE();
        auto b = getLocalBounds().toFloat();
        const float lv[2] { proc.getMeterL(), proc.getMeterR() };
        const float barH = (b.getHeight() - 6.f) * 0.5f;
        for (int i = 0; i < 2; ++i)
        {
            auto r = Rectangle<float> (b.getX(), b.getY() + 2.f + (float) i * (barH + 2.f), b.getWidth(), barH);
            g.setColour (col::bg0);
            g.fillRoundedRectangle (r, 2.f);
            const float db = Decibels::gainToDecibels (lv[i], -60.f);
            const float fill = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
            ColourGradient gr (col::green, r.getX(), 0.f, col::red, r.getRight(), 0.f, false);
            gr.addColour (0.75, col::amber);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * fill), 2.f);
        }
    }
private:
    void timerCallback() override
    {
        const float l = proc.getMeterL(), r = proc.getMeterR();
        if (std::abs (l - lastL) > 0.004f || std::abs (r - lastR) > 0.004f) { lastL = l; lastR = r; repaint(); }
    }
    float lastL = -1.f, lastR = -1.f;
    TransitFxProcessor& proc;
};

//==============================================================================
class ModulePanel : public Component
{
public:
    explicit ModulePanel (AudioProcessorValueTreeState& a) : apvts (a) { setModule (0); }
    ~ModulePanel() override { clearControls(); }

    void setModule (int m)
    {
        module = m;
        clearControls();
        const auto& spec = moduleSpecs()[(size_t) m];
        const Colour acc (spec.colour);

        power = std::make_unique<PowerButton> (acc);
        addAndMakeVisible (*power);
        powerAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, onId (m), *power);

        for (auto& p : spec.params)
        {
            const String id = paramId (m, p.id);
            if (p.kind == ParamSpec::Float)       knobs.push_back (std::make_unique<KnobComp> (apvts, id, p.name, acc));
            else if (p.kind == ParamSpec::Choice) choices.push_back (std::make_unique<ChoiceComp> (apvts, id, p.name, p.choices));
            else                                  pills.push_back (std::make_unique<PillAtt> (apvts, id, p.name, acc));
        }
        for (auto& k : knobs)   addAndMakeVisible (*k);
        for (auto& c : choices) addAndMakeVisible (*c);
        for (auto& p : pills)   addAndMakeVisible (p->button);
        resized();
        repaint();
    }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        const auto& spec = moduleSpecs()[(size_t) module];
        const Colour acc (spec.colour);
        auto r = getLocalBounds().toFloat();
        drawCard (g, r);

        g.setGradientFill (ColourGradient (acc.withAlpha (0.16f), r.getX(), r.getY(), Colours::transparentBlack, r.getX() + 360.f, r.getY(), false));
        g.fillRoundedRectangle (r.reduced (1.f).removeFromTop (88.f), 11.f);

        g.setColour (Colours::white);
        g.setFont (fnt (22.f, true));
        g.drawText (spec.name, 76, 16, 420, 28, Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (fnt (12.5f));
        g.drawText (spec.tag, 76, 42, 420, 18, Justification::centredLeft);

        g.setColour (col::dim.withAlpha (0.75f));
        g.setFont (fnt (10.5f));
        g.drawText ("POWER = ENGAGE  |  AUTOMATE THE POWER BUTTON IN FL STUDIO TO FIRE ON THE BEAT",
                    getLocalBounds().removeFromBottom (24), Justification::centred);
    }

    void resized() override
    {
        if (power == nullptr) return;
        auto b = getLocalBounds().reduced (20, 0);
        power->setBounds (20, 14, 46, 46);

        auto area = getLocalBounds().reduced (22, 0);
        area.removeFromTop (78);
        area.removeFromBottom (28);

        if (! choices.empty() || ! pills.empty())
        {
            auto row = area.removeFromTop (52);
            int x = row.getX();
            for (auto& c : choices) { c->setBounds (x, row.getY(), 150, 48); x += 162; }
            for (auto& p : pills)   { p->button.setBounds (x, row.getY() + 20, 118, 26); x += 130; }
            area.removeFromTop (6);
        }

        const int n = (int) knobs.size();
        if (n > 0)
        {
            const int kw = jmin (124, area.getWidth() / n);
            const int kh = jmin (area.getHeight(), 150);
            int x = area.getX() + (area.getWidth() - kw * n) / 2;
            const int y = area.getY() + (area.getHeight() - kh) / 2;
            for (auto& k : knobs) { k->setBounds (x, y, kw, kh); x += kw; }
        }
    }

private:
    struct PillAtt
    {
        PillAtt (AudioProcessorValueTreeState& a, const String& id, const String& name, Colour c) : button (name, c)
        {
            att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (a, id, button);
        }
        PillToggle button;
        std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> att;
    };

    void clearControls()
    {
        knobs.clear(); choices.clear(); pills.clear();
        powerAtt.reset(); power.reset();
    }

    AudioProcessorValueTreeState& apvts;
    int module = 0;
    std::unique_ptr<PowerButton> power;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<KnobComp>> knobs;
    std::vector<std::unique_ptr<ChoiceComp>> choices;
    std::vector<std::unique_ptr<PillAtt>> pills;
};

//==============================================================================
class ChainStrip : public Component, private Timer
{
public:
    std::function<void (int)> onSelect;

    ChainStrip (TransitFxProcessor& p) : proc (p) { startTimerHz (12); }

    void setSelected (int m) { selected = m; repaint(); }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        drawCard (g, getLocalBounds().toFloat());
        const auto order = proc.getOrder();
        const double t = (double) Time::getMillisecondCounter() * 0.004;

        for (int slot = 0; slot < kNumModules; ++slot)
        {
            const int m = order[(size_t) slot];
            if (dragging && m == dragModule) { drawTile (g, tileRect (slot), m, t, true, 0.25f); continue; }
            drawTile (g, tileRect (slot), m, t, false, 1.f);

            if (slot < kNumModules - 1)
            {
                auto tr = tileRect (slot);
                Path a;
                const float cx = tr.getRight() + 6.f, cy = tr.getCentreY();
                a.startNewSubPath (cx - 2.5f, cy - 5.f); a.lineTo (cx + 2.5f, cy); a.lineTo (cx - 2.5f, cy + 5.f);
                g.setColour (col::dim.withAlpha (0.7f));
                g.strokePath (a, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
            }
        }

        if (dragging)
        {
            auto r = tileRect (0).withX ((float) dragX - tileRect (0).getWidth() * 0.5f);
            drawTile (g, r, dragModule, t, true, 0.95f);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        const int slot = slotAt (e.x);
        if (slot < 0) return;
        const int m = proc.getOrder()[(size_t) slot];
        dragModule = m; dragSlot = slot; dragging = false;

        auto r = tileRect (slot);
        if (Rectangle<float> (r.getX(), r.getY(), 36.f, 34.f).contains (e.position))
        {
            toggle (m);
            dragModule = -1;
            return;
        }
        selected = m;
        if (onSelect) onSelect (m);
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragModule < 0) return;
        if (! dragging && std::abs (e.getDistanceFromDragStartX()) > 8) dragging = true;
        if (dragging) { dragX = e.x; repaint(); }
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (dragging && dragModule >= 0)
        {
            auto order = proc.getOrder();
            const int target = jlimit (0, kNumModules - 1, slotAtClamped (e.x));
            const int from = dragSlot;
            if (target != from)
            {
                const int m = order[(size_t) from];
                if (from < target) for (int i = from; i < target; ++i) order[(size_t) i] = order[(size_t) i + 1];
                else               for (int i = from; i > target; --i) order[(size_t) i] = order[(size_t) i - 1];
                order[(size_t) target] = m;
                proc.setOrder (order);
            }
        }
        dragging = false; dragModule = -1;
        repaint();
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const int slot = slotAt (e.x);
        if (slot >= 0) toggle (proc.getOrder()[(size_t) slot]);
    }

private:
    void timerCallback() override
    {
        juce::uint64 h = (juce::uint64) (selected + 1) * 1315423911ull;
        bool pulse = dragging;
        for (int m = 0; m < kNumModules; ++m)
        {
            const bool on = proc.apvts.getRawParameterValue (onId (m))->load() > 0.5f;
            const bool run = proc.isModuleRunning (m);
            h = h * 31 + (juce::uint64) (on ? 1 : 0) + (run ? 2 : 0) + (juce::uint64) proc.getOrder()[(size_t) m] * 7;
            if (run && ! on) pulse = true;
        }
        if (h != lastHash || pulse) { lastHash = h; repaint(); }
    }
    juce::uint64 lastHash = 0;

    float tileW() const { return ((float) getWidth() - 24.f) / (float) kNumModules; }
    Rectangle<float> tileRect (int slot) const
    {
        return { 12.f + (float) slot * tileW(), 8.f, tileW() - 14.f, (float) getHeight() - 16.f };
    }
    int slotAtClamped (int x) const { return jlimit (0, kNumModules - 1, (int) ((x - 12.f) / tileW())); }
    int slotAt (int x) const
    {
        const int s = (int) std::floor ((x - 12.f) / tileW());
        return (s >= 0 && s < kNumModules) ? s : -1;
    }

    void toggle (int m)
    {
        if (auto* p = proc.apvts.getParameter (onId (m)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.f : 1.f);
            p->endChangeGesture();
        }
    }

    void drawTile (Graphics& g, Rectangle<float> r, int m, double t, bool ghost, float alpha)
    {
        const auto& spec = moduleSpecs()[(size_t) m];
        const Colour acc (spec.colour);
        const bool on = proc.apvts.getRawParameterValue (onId (m))->load() > 0.5f;
        const bool running = proc.isModuleRunning (m);
        const bool sel = (m == selected);

        g.setOpacity (alpha);
        g.setColour (on ? acc.withAlpha (0.14f * alpha) : col::bg1.withAlpha (alpha));
        g.fillRoundedRectangle (r, 9.f);
        g.setColour ((sel ? acc : (on ? acc.withAlpha (0.55f) : col::stroke)).withAlpha (alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.f, sel ? 1.8f : 1.f);

        // LED
        const auto led = Point<float> (r.getX() + 18.f, r.getY() + 17.f);
        float glow = on ? 1.f : (running ? 0.35f + 0.25f * (float) std::sin (t * 6.0) : 0.f);
        if (glow > 0.f)
        {
            g.setColour (acc.withAlpha (0.28f * glow * alpha));
            g.fillEllipse (led.x - 9.f, led.y - 9.f, 18.f, 18.f);
        }
        g.setColour ((glow > 0.f ? acc.withAlpha (0.45f + 0.55f * glow) : col::stroke).withAlpha (alpha));
        g.fillEllipse (led.x - 4.5f, led.y - 4.5f, 9.f, 9.f);

        g.setColour (col::dim.withAlpha (alpha));
        g.setFont (fnt (10.f, true));
        g.drawText (String (m + 1).paddedLeft ('0', 2), r.withTrimmedRight (9.f).removeFromTop (30.f), Justification::centredRight);

        g.setColour ((on || sel ? Colours::white : col::text.withAlpha (0.8f)).withAlpha (alpha));
        g.setFont (fnt (11.f, true));
        g.drawText (spec.name, r.withTrimmedTop (30.f).reduced (4.f, 2.f), Justification::centred);
        juce::ignoreUnused (ghost);
    }

    TransitFxProcessor& proc;
    int selected = 0, dragModule = -1, dragSlot = 0, dragX = 0;
    bool dragging = false;
};

//==============================================================================
class TransitionPanel : public Component, private Timer
{
public:
    explicit TransitionPanel (TransitFxProcessor& p) : proc (p)
    {
        static const uint32 cols[] { 0xff35d7ff, 0xff8a63ff, 0xff4d8dff, 0xffff7a45, 0xffff3d8b,
                                     0xffffb547, 0xff3de8a0, 0xff7fe7ff, 0xffc06cff, 0xffff3b4e };
        const auto& sc = transitionScenes();
        for (size_t i = 0; i < sc.size(); ++i)
        {
            auto b = std::make_unique<SceneButton> (sc[i].name, Colour (cols[i % 10]));
            b->onClick = [this, i]
            {
                if (active == (int) i) { proc.turnAllModulesOff(); active = -1; }
                else                   { proc.applyScene (transitionScenes()[i]); active = (int) i; }
                refreshToggles();
            };
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
        }
        startTimerHz (6);
    }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        auto r = getLocalBounds().toFloat();
        drawCard (g, r);
        g.setColour (Colours::white);
        g.setFont (fnt (13.f, true));
        g.drawText ("ONE CLICK TRANSITIONS", 20, 14, getWidth() - 40, 20, Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (fnt (10.5f));
        g.drawText ("Click to load and fire a combination. Click again to release.", 20, 34, getWidth() - 40, 16, Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (14, 0);
        b.removeFromTop (58);
        b.removeFromBottom (14);
        const int rows = 5, gap = 8;
        const int bw = (b.getWidth() - gap) / 2;
        const int bh = (b.getHeight() - gap * (rows - 1)) / rows;
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            const int cx = (int) (i % 2), cy = (int) (i / 2);
            buttons[i]->setBounds (b.getX() + cx * (bw + gap), b.getY() + cy * (bh + gap), bw, bh);
        }
    }

private:
    void refreshToggles()
    {
        for (size_t i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState ((int) i == active, dontSendNotification);
    }
    void timerCallback() override
    {
        if (active < 0) return;
        bool any = false;
        for (int m = 0; m < kNumModules; ++m)
            if (proc.apvts.getRawParameterValue (onId (m))->load() > 0.5f) any = true;
        if (! any) { active = -1; refreshToggles(); }
    }

    TransitFxProcessor& proc;
    std::vector<std::unique_ptr<SceneButton>> buttons;
    int active = -1;
};

//==============================================================================
class WaveformView : public Component, private Timer
{
public:
    explicit WaveformView (TransitFxProcessor& p) : proc (p) { startTimerHz (15); }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        auto& rec = proc.getRecorder();
        const auto st = rec.getState();
        auto r = getLocalBounds().toFloat();

        g.setColour (col::bg0);
        g.fillRoundedRectangle (r, 9.f);

        // grid
        g.setColour (Colours::white.withAlpha (0.035f));
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine ((int) (r.getWidth() * (float) i / 8.f), r.getY() + 6.f, r.getBottom() - 6.f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 6.f, r.getRight() - 6.f);

        const bool live = (st == TransitRecorder::State::Recording || st == TransitRecorder::State::Finishing);
        if (peaks.empty() && rec.getLength() > 0) rebuildPeaks (live);

        if (st == TransitRecorder::State::Empty)
        {
            g.setColour (col::dim);
            g.setFont (fnt (13.f));
            g.drawText ("Press REC, play your audio, then STOP. The processed signal is captured with reverb and delay tails.",
                        getLocalBounds().reduced (24, 0), Justification::centred);
        }
        else if (! peaks.empty())
        {
            const float halfH = r.getHeight() * 0.5f - 10.f;
            const float cy = r.getCentreY();
            ColourGradient gr (live ? col::red : col::cyan, 0.f, 0.f, live ? col::amber : col::violet, r.getRight(), 0.f, false);
            g.setGradientFill (gr);
            const float scale = (float) getWidth() / (float) jmax (1, stretchBins);
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float h = jmax (1.f, peaks[i] * halfH);
                g.fillRect ((float) i * scale, cy - h, jmax (1.f, scale - 0.4f), h * 2.f);
            }

            if (rec.isPlaying())
            {
                const float x = (float) rec.getPlayPos() / (float) jmax (1, rec.getLength()) * r.getWidth();
                g.setColour (Colours::white);
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            }
        }

        const bool ready = (st == TransitRecorder::State::Ready);
        const bool hot = ready && (isMouseOver() || dragActive);
        g.setColour (hot ? col::cyan : (live ? col::red.withAlpha (0.7f) : col::stroke));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.f, hot ? 1.8f : 1.f);

        if (ready)
        {
            const String hint = hot ? "DRAG INTO FL STUDIO PLAYLIST" : "DRAG THIS WAVEFORM TO THE PLAYLIST";
            g.setFont (fnt (10.5f, true));
            const int tw = (int) (fnt (10.5f, true).getStringWidthFloat (hint)) + 22;
            auto tag = Rectangle<float> ((float) tw, 20.f).withRightX (r.getRight() - 10.f).withY (r.getY() + 8.f);
            g.setColour (col::bg0.withAlpha (0.85f));
            g.fillRoundedRectangle (tag, 10.f);
            g.setColour (hot ? col::cyan : col::dim);
            g.drawText (hint, tag, Justification::centred);
        }
        else if (st == TransitRecorder::State::Finishing)
        {
            g.setColour (col::amber);
            g.setFont (fnt (10.5f, true));
            g.drawText ("CAPTURING TAIL...", getLocalBounds().reduced (12, 8).removeFromTop (16), Justification::centredRight);
        }
    }

    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override  { repaint(); }

    void mouseDrag (const MouseEvent& e) override
    {
        auto& rec = proc.getRecorder();
        if (dragActive || rec.getState() != TransitRecorder::State::Ready) return;
        if (e.getDistanceFromDragStart() < 8) return;

        dragActive = true;
        const File f = rec.getOrCreateFile (proc.getExportSampleRate());
        if (f.existsAsFile())
            DragAndDropContainer::performExternalDragDropOfFiles (StringArray (f.getFullPathName()), false, this, nullptr);
        dragActive = false;
        repaint();
    }

    MouseCursor getMouseCursor() override
    {
        return proc.getRecorder().getState() == TransitRecorder::State::Ready ? MouseCursor::DraggingHandCursor
                                                                              : MouseCursor::NormalCursor;
    }

private:
    void timerCallback() override
    {
        auto& rec = proc.getRecorder();
        const int len = rec.getLength();
        const int st = (int) rec.getState();
        const bool live = st == (int) TransitRecorder::State::Recording || st == (int) TransitRecorder::State::Finishing;
        if (len != lastLen || st != lastState || getWidth() != lastW || rec.isPlaying())
        {
            lastLen = len; lastState = st; lastW = getWidth();
            rebuildPeaks (live);
            repaint();
        }
    }

    void rebuildPeaks (bool live)
    {
        auto& rec = proc.getRecorder();
        const int len = rec.getLength();
        peaks.clear();
        if (len <= 0 || getWidth() <= 0) return;

        const double sr = jmax (1.0, rec.getRecordedSR());
        const double span = live ? jmax ((double) len, 8.0 * sr) : (double) len;
        const int bins = jmax (1, getWidth() / 2);
        const int usedBins = jmin (bins, (int) std::ceil ((double) len / span * (double) bins));
        peaks.assign ((size_t) usedBins, 0.f);
        for (int b = 0; b < usedBins; ++b)
        {
            const int s0 = (int) ((double) b * span / (double) bins);
            const int s1 = jmin (len, (int) ((double) (b + 1) * span / (double) bins));
            const int stride = jmax (1, (s1 - s0) / 40);
            float pk = 0.f;
            for (int i = s0; i < s1; i += stride)
                pk = jmax (pk, std::abs (rec.peek (0, i)), std::abs (rec.peek (1, i)));
            peaks[(size_t) b] = jlimit (0.f, 1.f, pk);
        }
        // stretch so that bars cover the area proportionally
        stretchBins = bins;
    }

    TransitFxProcessor& proc;
    std::vector<float> peaks;
    int stretchBins = 1, lastLen = -1, lastState = -1, lastW = -1;
    bool dragActive = false;
};

//==============================================================================
class RecorderPanel : public Component, private Timer
{
public:
    explicit RecorderPanel (TransitFxProcessor& p)
        : proc (p), wave (p), bRec (TransportButton::Rec), bStop (TransportButton::Stop),
          bPlay (TransportButton::Play), bDel (TransportButton::Del),
          trim ("Auto-trim", col::cyan)
    {
        addAndMakeVisible (wave);
        for (auto* b : { &bRec, &bStop, &bPlay, &bDel }) addAndMakeVisible (*b);

        bRec.onClick  = [this] { proc.recordStart(); };
        bStop.onClick = [this]
        {
            auto& r = proc.getRecorder();
            if (r.getState() == TransitRecorder::State::Recording) proc.recordStop();
            else r.stopPreview();
        };
        bPlay.onClick = [this]
        {
            auto& r = proc.getRecorder();
            if (r.isPlaying()) r.stopPreview(); else r.startPreview();
        };
        bDel.onClick = [this] { proc.getRecorder().clear(); };

        exportBtn.setButtonText ("EXPORT WAV");
        exportBtn.onClick = [this] { exportWav(); };
        addAndMakeVisible (exportBtn);

        srBox.addItemList ({ "Host rate", "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" }, 1);
        addAndMakeVisible (srBox);
        srAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "rec_sr", srBox);

        tail.setSliderStyle (Slider::LinearHorizontal);
        tail.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        tail.setColour (Slider::trackColourId, col::amber);
        addAndMakeVisible (tail);
        tailAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "rec_tail", tail);

        addAndMakeVisible (trim);
        trimAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "rec_trim", trim);

        startTimerHz (10);
    }

    ~RecorderPanel() override { stopTimer(); }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        drawCard (g, getLocalBounds().toFloat());
        auto& rec = proc.getRecorder();
        const auto st = rec.getState();

        g.setColour (Colours::white);
        g.setFont (fnt (13.f, true));
        g.drawText ("RECORDER", 20, 12, 120, 20, Justification::centredLeft);

        String status = "EMPTY";
        Colour sc = col::dim;
        if (st == TransitRecorder::State::Recording) { status = "RECORDING"; sc = col::red; }
        else if (st == TransitRecorder::State::Finishing) { status = "CAPTURING TAIL"; sc = col::amber; }
        else if (st == TransitRecorder::State::Ready) { status = rec.isPlaying() ? "PLAYING" : "READY"; sc = col::green; }
        g.setColour (sc);
        g.setFont (fnt (11.f, true));
        g.drawText (status, 118, 12, 160, 20, Justification::centredLeft);

        const double sr = jmax (1.0, rec.getRecordedSR());
        g.setColour (col::text);
        g.setFont (fnt (14.f, true));
        g.drawText (formatTime ((double) rec.getLength() / sr), 20, 34, 120, 20, Justification::centredLeft);

        g.setColour (col::dim);
        g.setFont (fnt (9.5f, true));
        g.drawText ("EXPORT RATE", srBox.getX(), srBox.getY() - 13, 100, 12, Justification::centredLeft);
        g.drawText ("TAIL " + fmtValue ((float) tail.getValue(), "s"), 96, 35, 60, 16, Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int lx = 20;
        int x = lx;
        for (auto* tb : { &bRec, &bStop, &bPlay, &bDel }) { tb->setBounds (x, 58, 42, 42); x += 50; }
        exportBtn.setBounds (lx, 108, 192, 28);
        srBox.setBounds (lx, 154, 104, 22);
        trim.setBounds (lx + 112, 154, 80, 22);
        tail.setBounds (158, 34, 74, 18);

        auto wv = b.withTrimmedLeft (236).reduced (0, 14);
        wv.removeFromRight (16);
        wave.setBounds (wv);
    }

private:
    void timerCallback() override
    {
        auto& r = proc.getRecorder();
        const auto st = r.getState();
        const bool busy = st == TransitRecorder::State::Recording || st == TransitRecorder::State::Finishing;
        const bool ready = st == TransitRecorder::State::Ready;

        bRec.setEnabled (! busy);
        bStop.setEnabled (busy || r.isPlaying());
        bPlay.setEnabled (ready);
        bPlay.glyphStop = r.isPlaying();
        bDel.setEnabled (st != TransitRecorder::State::Empty && ! busy);
        exportBtn.setEnabled (ready);
        bRec.pulsing = busy;

        // render the draggable file as soon as the recording is finished
        if (ready && ! preRendered)
        {
            r.getOrCreateFile (proc.getExportSampleRate());
            preRendered = true;
        }
        if (! ready) preRendered = false;

        const int key = (int) st + 8 * (r.isPlaying() ? 1 : 0) + 16 * (r.getLength() / 4096) + 7919 * (r.getPlayPos() / 8192);
        if (key != lastKey || busy)
        {
            lastKey = key;
            for (auto* b : { &bRec, &bStop, &bPlay, &bDel }) b->repaint();
            repaint();
        }
    }
    int lastKey = -1;

    void exportWav()
    {
        auto& r = proc.getRecorder();
        if (r.getState() != TransitRecorder::State::Ready) return;
        auto def = TransitRecorder::nextUniqueFile();
        chooser = std::make_unique<FileChooser> ("Export WAV (24-bit)", def, "*.wav");
        SafePointer<RecorderPanel> safe (this);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [safe] (const FileChooser& fc)
            {
                if (safe == nullptr) return;
                auto f = fc.getResult();
                if (f == File()) return;
                f = f.withFileExtension ("wav");
                safe->proc.getRecorder().renderToFile (f, safe->proc.getExportSampleRate());
            });
    }

    TransitFxProcessor& proc;
    WaveformView wave;
    TransportButton bRec, bStop, bPlay, bDel;
    TextButton exportBtn;
    ComboBox srBox;
    Slider tail;
    PillToggle trim;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> srAtt;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> tailAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> trimAtt;
    std::unique_ptr<FileChooser> chooser;
    bool preRendered = false;
};

class SafeShell : public Component
{
public:
    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff06080c));
        g.setColour (Colours::white);
        g.setFont (Font (FontOptions (18.f)));
        g.drawText ("STEINEX TRANSIT FX  -  SAFE MODE (ui_level 0)", getLocalBounds(), Justification::centred);
    }
};

//==============================================================================
class MainComponent : public Component
{
public:
    MainComponent (TransitFxProcessor& p, bool custom)
        : proc (p), chain (p), modulePanel (p.apvts), transitions (p), recorderPanel (p), meter (p),
          outKnob (p.apvts, "g_out", "Output", col::cyan),
          mixKnob (p.apvts, "g_mix", "Wet / Dry", col::violet),
          bypass ("Bypass", col::red)
    {
        customLnF = custom;
        if (customLnF) setLookAndFeel (&lnf);
        steinex::logLine ("main: constructing children done");
        setSize (1100, 720);

        addAndMakeVisible (chain);
        addAndMakeVisible (modulePanel);
        addAndMakeVisible (transitions);
        addAndMakeVisible (recorderPanel);
        addAndMakeVisible (meter);
        addAndMakeVisible (outKnob);
        addAndMakeVisible (mixKnob);
        addAndMakeVisible (bypass);
        bypassAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "g_bypass", bypass);

        chain.onSelect = [this] (int m) { modulePanel.setModule (m); };

        addAndMakeVisible (presetBox);
        addAndMakeVisible (prevBtn);
        addAndMakeVisible (nextBtn);
        addAndMakeVisible (saveBtn);
        prevBtn.setButtonText ("<");
        nextBtn.setButtonText (">");
        saveBtn.setButtonText ("SAVE");
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };
        saveBtn.onClick = [this] { savePreset(); };
        presetBox.onChange = [this] { presetChosen(); };
        refreshPresets (-1);
        addMouseListener (this, true);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (mouseLogs++ < 40)
            steinex::logLine (String ("ui: mouseDown on ") + typeid (*e.eventComponent).name() + " at " + String (e.x) + "," + String (e.y));
    }
    void mouseEnter (const MouseEvent& e) override
    {
        if (! loggedEnter) { loggedEnter = true; steinex::logLine (String ("ui: first mouseEnter on ") + typeid (*e.eventComponent).name()); }
    }

    ~MainComponent() override
    {
        if (alert != nullptr) alert->exitModalState (0);
        if (customLnF) setLookAndFeel (nullptr);
    }

    void paintBackground (Graphics& g)
    {
        auto b = Rectangle<float> (0.f, 0.f, 1100.f, 720.f);
        g.setGradientFill (ColourGradient (Colour (0xff0b0f17), 0.f, 0.f, col::bg0, 0.f, b.getHeight(), false));
        g.fillAll();
        g.setGradientFill (ColourGradient (col::violet.withAlpha (0.10f), b.getWidth() * 0.85f, 0.f, Colours::transparentBlack, b.getWidth() * 0.85f, 420.f, true));
        g.fillRect (b);
        g.setGradientFill (ColourGradient (col::cyan.withAlpha (0.08f), 80.f, 0.f, Colours::transparentBlack, 80.f, 360.f, true));
        g.fillRect (b);

        // header
        drawLogoMark (g, { 26.f, 22.f, 46.f, 46.f });
        g.setColour (Colours::white);
        drawSpaced (g, "STEINEX", 86.f, 16.f, 34.f, 5.f, true, 26.f);
        g.setGradientFill (ColourGradient (col::cyan, 86.f, 0.f, col::violet, 230.f, 0.f, false));
        drawSpaced (g, "TRANSIT FX", 87.f, 48.f, 20.f, 5.5f, true, 14.f);

        g.setColour (col::stroke);
        g.drawHorizontalLine (92, 24.f, b.getWidth() - 24.f);

        g.setColour (col::dim.withAlpha (0.7f));
        g.setFont (fnt (10.f, true));
        g.drawText ("STEINEX TRANSIT FX   |   64-BIT VST3   |   ORIGINAL STEINEX DESIGN", 24, 700, 1100 - 48, 16, Justification::centred);
    }

    void paint (Graphics& g) override
    {
        STX_ONCE();
        if (bg.isNull()) { bg = Image (Image::ARGB, 1100, 720, true); Graphics bgG (bg); paintBackground (bgG); }
        g.drawImageAt (bg, 0, 0);
    }

    void resized() override
    {
        presetBox.setBounds (372, 34, 230, 30);
        prevBtn.setBounds (334, 34, 30, 30);
        nextBtn.setBounds (610, 34, 30, 30);
        saveBtn.setBounds (652, 34, 62, 30);

        outKnob.setBounds (754, 4, 82, 84);
        mixKnob.setBounds (838, 4, 82, 84);
        meter.setBounds (934, 30, 62, 32);
        bypass.setBounds (1006, 34, 70, 26);

        chain.setBounds (24, 102, 1052, 80);
        modulePanel.setBounds (24, 194, 716, 308);
        transitions.setBounds (756, 194, 320, 308);
        recorderPanel.setBounds (24, 514, 1052, 180);
    }

private:
    void refreshPresets (int selectId)
    {
        presetBox.clear (dontSendNotification);
        presetBox.addSectionHeading ("FACTORY");
        const auto& ps = presetScenes();
        for (size_t i = 0; i < ps.size(); ++i) presetBox.addItem (ps[i].name, (int) i + 1);
        userFiles = TransitFxProcessor::listUserPresets();
        if (userFiles.size() > 0)
        {
            presetBox.addSeparator();
            presetBox.addSectionHeading ("USER");
            for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 101 + i);
        }
        if (selectId > 0) presetBox.setSelectedId (selectId, dontSendNotification);
        else              presetBox.setText ("Select preset", dontSendNotification);
    }

    void presetChosen()
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id <= (int) presetScenes().size())      proc.applyScene (presetScenes()[(size_t) id - 1]);
        else if (id >= 101 && id - 101 < userFiles.size())     proc.loadUserPreset (userFiles[id - 101]);
    }

    void stepPreset (int dir)
    {
        const int total = (int) presetScenes().size();
        int id = presetBox.getSelectedId();
        if (id <= 0) id = dir > 0 ? 0 : total + 1;
        id += dir;
        if (id < 1) id = total;
        if (id > total && id < 101) id = 1;
        presetBox.setSelectedId (id, sendNotificationSync);
    }

    void savePreset()
    {
        auto* aw = new AlertWindow ("Save combination", "Name your custom combination. It stores every effect setting.",
                                    MessageBoxIconType::NoIcon, this);
        if (customLnF) aw->setLookAndFeel (&lnf);
        aw->addTextEditor ("name", "My Transition", "Name");
        aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        alert = aw;
        SafePointer<MainComponent> safe (this);
        aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const String name = aw->getTextEditorContents ("name");
            if (safe->proc.saveUserPreset (name))
            {
                safe->refreshPresets (-1);
                for (int i = 0; i < safe->userFiles.size(); ++i)
                    if (safe->userFiles[i].getFileNameWithoutExtension() == File::createLegalFileName (name.trim()))
                        safe->presetBox.setSelectedId (101 + i, dontSendNotification);
            }
        }), true);
    }

    TransitFxProcessor& proc;
    bool customLnF = true;
    SteinexLnF lnf;
    ChainStrip chain;
    ModulePanel modulePanel;
    TransitionPanel transitions;
    RecorderPanel recorderPanel;
    MeterView meter;
    KnobComp outKnob, mixKnob;
    PillToggle bypass;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    ComboBox presetBox;
    TextButton prevBtn, nextBtn, saveBtn;
    Array<File> userFiles;
    Component::SafePointer<AlertWindow> alert;
    Image bg;
    int mouseLogs = 0;
    bool loggedEnter = false;
};
} // namespace

//==============================================================================
TransitFxEditor::TransitFxEditor (TransitFxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    steinex::logLine ("editor: constructing");
    setOpaque (true);
    int level = 2;
    {
        auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("STEINEX").getChildFile ("TRANSIT FX").getChildFile ("ui_level.txt");
        if (f.existsAsFile()) level = f.loadFileAsString().trim().getIntValue();
    }
    steinex::logLine ("editor: ui level " + String (level));
    if (level <= 0) content = std::make_unique<SafeShell>();
    else            content = std::make_unique<MainComponent> (p, level >= 2);
    addAndMakeVisible (*content);
    Desktop::getInstance().addGlobalMouseListener (&clickLogger);
    setResizable (false, false);
    setSize (baseW, baseH);
    startTimer (60);
    steinex::logLine ("editor: constructed");
}

TransitFxEditor::~TransitFxEditor()
{
    stopTimer();
    Desktop::getInstance().removeGlobalMouseListener (&clickLogger);
    steinex::logLine ("editor: destroyed");
}

// Use the plain software renderer: it is the most compatible one inside DAW windows.
void TransitFxEditor::timerCallback()
{
    if (auto* peer = getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        int want = -1;
        for (int i = 0; i < engines.size(); ++i)
            if (engines[i].containsIgnoreCase ("software")) { want = i; break; }
        if (want >= 0 && peer->getCurrentRenderingEngine() != want)
            peer->setCurrentRenderingEngine (want);
        steinex::logLine ("editor: renderer engines [" + engines.joinIntoString (", ") + "], using " + String (peer->getCurrentRenderingEngine()));
        stopTimer();
    }
    else if (++tries > 100)
        stopTimer();
}

void TransitFxEditor::paint (Graphics& g)
{
    if (firstPaint) { firstPaint = false; steinex::logLine ("editor: first paint"); }
    g.fillAll (Colour (0xff06080c));
}

void TransitFxEditor::resized() { content->setBounds (getLocalBounds()); }

Image TransitFxEditor::renderSnapshot (float scale)
{
    return content->createComponentSnapshot (content->getLocalBounds(), true, scale);
}

void ClickLogger::mouseDown (const MouseEvent& e)
{
    if (++n <= 150 && e.eventComponent != nullptr)
        steinex::logLine ("click down: " + String (typeid (*e.eventComponent).name()) + " @" + String (e.x) + "," + String (e.y));
}

void ClickLogger::mouseUp (const MouseEvent& e)
{
    if (n <= 150 && e.eventComponent != nullptr)
        steinex::logLine ("click up: " + String (typeid (*e.eventComponent).name()));
}
