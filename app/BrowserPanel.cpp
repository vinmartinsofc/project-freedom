#include "BrowserPanel.h"
#include <algorithm>

BrowserPanel::BrowserPanel (PluginScanner& s) : scanner (s)
{
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::text());
    addAndMakeVisible (title);

    search.setTextToShowWhenEmpty ("Buscar (nome ou fabricante)", theme::textDim());
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    listBox.setRowHeight (24);
    listBox.setColour (juce::ListBox::backgroundColourId, theme::panel());
    addAndMakeVisible (listBox);

    for (auto* b : { &allButton, &instrumentsButton, &effectsButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setMouseClickGrabsKeyboardFocus (false);
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent());
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (b);
    }
    allButton.setToggleState (true, juce::dontSendNotification);
    allButton.onClick         = [this] { setFilter (Filter::All); };
    instrumentsButton.onClick = [this] { setFilter (Filter::Instruments); };
    effectsButton.onClick     = [this] { setFilter (Filter::Effects); };

    rescanButton.onClick = [this]
    {
        setStatus ("Procurando plugins...");
        scanner.startScan();
    };
    addAndMakeVisible (rescanButton);

    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, theme::textDim());
    addAndMakeVisible (status);

    scanner.list().addChangeListener (this);
    refresh();
}

BrowserPanel::~BrowserPanel() { scanner.list().removeChangeListener (this); }

void BrowserPanel::setFilter (Filter f) { filter = f; refresh(); }

void BrowserPanel::refresh()
{
    const auto text = search.getText().trim();
    shown.clear();
    for (const auto& d : scanner.list().getTypes())
    {
        const bool kindOk = filter == Filter::All || (filter == Filter::Instruments) == d.isInstrument;
        if (kindOk && (text.isEmpty() || d.name.containsIgnoreCase (text) || d.manufacturerName.containsIgnoreCase (text)))
            shown.push_back (d);
    }

    std::sort (shown.begin(), shown.end(), [] (const auto& a, const auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    listBox.updateContent();
    listBox.repaint();
}

void BrowserPanel::load (int row)
{
    if (row >= 0 && row < static_cast<int> (shown.size()) && onLoadRequested)
        onLoadRequested (shown[static_cast<size_t> (row)]);
}

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (shown.size())) return;
    const auto& d = shown[static_cast<size_t> (row)];

    if (selected) g.fillAll (theme::accent().withAlpha (0.25f));
    g.setColour (theme::text());
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (d.name, 10, 0, width - 20, height, juce::Justification::centredLeft, true);

    g.setColour (theme::textDim());
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (d.manufacturerName, 10, 0, width - 20, height, juce::Justification::centredRight, true);

    if (! d.isInstrument)
    {
        g.setColour (juce::Colour (0xff8c7ae6));
        g.fillRoundedRectangle (static_cast<float> (width - 30), 3.0f, 3.0f, static_cast<float> (height - 6), 1.5f);   // marca de efeito
    }
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    g.setColour (theme::border());
    g.drawVerticalLine (getWidth() - 1, 0.0f, static_cast<float> (getHeight()));
}

void BrowserPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto head = area.removeFromTop (24);
    rescanButton.setBounds (head.removeFromRight (64));
    title.setBounds (head);
    area.removeFromTop (6);
    search.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    auto tabs = area.removeFromTop (24);
    allButton.setBounds (tabs.removeFromLeft (52));
    tabs.removeFromLeft (3);
    instrumentsButton.setBounds (tabs.removeFromLeft (92));
    tabs.removeFromLeft (3);
    effectsButton.setBounds (tabs);
    area.removeFromTop (6);
    status.setBounds (area.removeFromBottom (20));
    listBox.setBounds (area);
}
