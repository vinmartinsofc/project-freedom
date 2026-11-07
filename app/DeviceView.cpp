#include "DeviceView.h"

class DeviceView::Box : public juce::Component
{
public:
    Box (const DeviceInfo& d, DeviceView& o) : info (d), owner (o)
    {
        addAndMakeVisible (openButton);
        addAndMakeVisible (removeButton);
        openButton.onClick = [this] { if (owner.onOpen) owner.onOpen (owner.trackId, info.id); };
        removeButton.onClick = [this]
        {
            const int tid = owner.trackId, did = info.id;
            auto cb = owner.onRemove;                                // o Box será destruído no callback
            juce::MessageManager::callAsync ([cb, tid, did] { if (cb) cb (tid, did); });
        };
        openButton.setMouseClickGrabsKeyboardFocus (false);
        removeButton.setMouseClickGrabsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (theme::panelLight());
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (info.isInstrument ? theme::accent() : juce::Colour (0xff8c7ae6));
        g.fillRoundedRectangle (r.withHeight (5.0f), 3.0f);
        g.setColour (theme::text());
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (info.name, getLocalBounds().reduced (10, 0).withTrimmedTop (12).withHeight (36), juce::Justification::centredLeft, true);
        g.setColour (theme::textDim());
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (info.isInstrument ? theme::ui ("Instrumento") : theme::ui ("Efeito"), 10, 46, getWidth() - 20, 16, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (8).removeFromBottom (26);
        removeButton.setBounds (a.removeFromRight (30));
        a.removeFromRight (4);
        openButton.setBounds (a);
    }

private:
    DeviceInfo info;
    DeviceView& owner;
    juce::TextButton openButton { "Abrir" }, removeButton { "x" };
};

DeviceView::DeviceView() = default;
DeviceView::~DeviceView() = default;

void DeviceView::setDevices (int id, const juce::String& name, const std::vector<DeviceInfo>& devices)
{
    trackId = id;
    trackName = name;
    boxes.clear();
    removeAllChildren();
    for (const auto& d : devices)
    {
        boxes.push_back (std::make_unique<Box> (d, *this));
        addAndMakeVisible (*boxes.back());
    }
    resized();
    repaint();
}

void DeviceView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    g.setColour (theme::textDim());
    g.setFont (juce::FontOptions (12.0f));
    if (boxes.empty())
        g.drawFittedText (theme::ui ("Trilha \"") + trackName + theme::ui ("\" sem dispositivos.\nDê duplo clique em um plugin da lista para adicionar.\n(O instrumento vai primeiro; efeitos entram em sequência.)"),
                          getLocalBounds().reduced (20), juce::Justification::centred, 4);
}

void DeviceView::resized()
{
    auto a = getLocalBounds().reduced (8);
    for (auto& b : boxes)
    {
        b->setBounds (a.removeFromLeft (190));
        a.removeFromLeft (6);
    }
}
