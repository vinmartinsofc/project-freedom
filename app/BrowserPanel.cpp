#include "BrowserPanel.h"
#include "PluginFolderGrouping.h"
#include <algorithm>

class BrowserPanel::TreeItem : public juce::TreeViewItem
{
public:
    TreeItem (BrowserPanel& p, juce::String label, int itemCount)
        : owner (p), text (std::move (label)), count (itemCount), isFolder (true) {}

    TreeItem (BrowserPanel& p, const juce::PluginDescription& d)
        : owner (p), description (d), text (d.name) {}

    bool mightContainSubItems() override { return isFolder; }

    juce::String getUniqueName() const override
    {
        return isFolder ? "folder:" + text
                        : "plugin:" + description.fileOrIdentifier + ":" + description.name;
    }

    juce::String getTooltip() override
    {
        return isFolder ? text + " (" + juce::String (count) + ")"
                        : description.name + " — " + description.manufacturerName;
    }

    juce::var getDragSourceDescription() override
    {
        return isFolder ? juce::var()
                        : juce::var ("plugin:" + description.fileOrIdentifier);
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
            g.fillAll (theme::accent().withAlpha (0.25f));

        g.setColour (theme::text());
        g.setFont (juce::FontOptions (isFolder && getParentItem() == nullptr ? 13.0f : 12.0f,
                                      isFolder ? juce::Font::bold : juce::Font::plain));

        if (isFolder)
        {
            g.drawText (text + " (" + juce::String (count) + ")", 4, 0, width - 8, height,
                        juce::Justification::centredLeft, true);
            return;
        }

        g.drawText (description.name, 4, 0, std::max (0, width - 86), height,
                    juce::Justification::centredLeft, true);
        g.setColour (theme::textDim());
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (description.manufacturerName, std::max (0, width - 82), 0, 68, height,
                    juce::Justification::centredRight, true);

        if (! description.isInstrument)
        {
            g.setColour (juce::Colour (0xff8c7ae6));
            g.fillRoundedRectangle (static_cast<float> (width - 7), 4.0f, 3.0f,
                                    static_cast<float> (height - 8), 1.5f);
        }
    }

    void itemDoubleClicked (const juce::MouseEvent& event) override
    {
        if (isFolder)
            juce::TreeViewItem::itemDoubleClicked (event);
        else
            owner.load (description);
    }

    void itemOpennessChanged (bool) override
    {
        if (isFolder)
            owner.saveTreeState();
    }

    bool isPlugin() const { return ! isFolder; }
    const juce::PluginDescription& plugin() const { return description; }

private:
    BrowserPanel& owner;
    juce::PluginDescription description;
    juce::String text;
    int count = 0;
    bool isFolder = false;
};

class BrowserPanel::PluginTree : public juce::TreeView
{
public:
    explicit PluginTree (BrowserPanel& p) : owner (p) {}

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey && owner.loadSelectedPlugin())
            return true;

        return juce::TreeView::keyPressed (key);
    }

private:
    BrowserPanel& owner;
};

BrowserPanel::BrowserPanel (PluginScanner& s) : scanner (s), tree (std::make_unique<PluginTree> (*this))
{
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::text());
    addAndMakeVisible (title);

    search.setTextToShowWhenEmpty ("Buscar (nome ou fabricante)", theme::textDim());
    search.onTextChange = [this] { refresh(); };
    search.addKeyListener (this);
    addAndMakeVisible (search);

    tree->setColour (juce::TreeView::backgroundColourId, theme::panel());
    tree->setRootItemVisible (true);
    tree->addKeyListener (this);
    addAndMakeVisible (*tree);

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

BrowserPanel::~BrowserPanel()
{
    saveTreeState();
    scanner.list().removeChangeListener (this);
}

void BrowserPanel::grabFocus()
{
    tree->grabKeyboardFocus();
}

bool BrowserPanel::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key.getModifiers().isCommandDown()
        && juce::CharacterFunctions::toLowerCase (key.getKeyCode()) == 'f')
    {
        search.grabKeyboardFocus();
        return true;
    }
    return false;
}

void BrowserPanel::setFilter (Filter f)
{
    filter = f;
    refresh();
}

juce::File BrowserPanel::treeStateFile() const
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("ProjectFreedom");
    directory.createDirectory();
    return directory.getChildFile ("plugin-tree-state.xml");
}

void BrowserPanel::saveTreeState()
{
    if (searchActive || rebuildingTree || tree == nullptr || tree->getRootItem() == nullptr)
        return;

    if (auto state = tree->getOpennessState (true))
        state->writeTo (treeStateFile());
}

void BrowserPanel::refresh()
{
    const auto query = search.getText().trim();
    const bool searching = query.isNotEmpty();
    const bool wasSearching = searchActive;

    if (searching && ! wasSearching)
        opennessBeforeSearch = tree->getOpennessState (true);
    else if (! searching && ! wasSearching)
        saveTreeState();

    searchActive = searching;
    rebuildingTree = true;

    struct Group
    {
        juce::String name;
        std::vector<juce::PluginDescription> plugins;
    };

    std::vector<juce::PluginDescription> loosePlugins;
    std::vector<Group> groups;
    const auto roots = scanner.searchRoots();

    for (const auto& description : scanner.list().getTypes())
    {
        const bool kindMatches = filter == Filter::All
                              || (filter == Filter::Instruments) == description.isInstrument;
        const bool textMatches = query.isEmpty()
                              || description.name.containsIgnoreCase (query)
                              || description.manufacturerName.containsIgnoreCase (query);
        if (! kindMatches || ! textMatches)
            continue;

        juce::String folder;
        const roll::PluginPath path { description.name.toStdString(),
                                      description.fileOrIdentifier.toStdString() };
        for (const auto& root : roots)
        {
            const auto grouped = roll::groupPluginsByFolder ({ path }, root.getFullPathName().toStdString());
            if (! grouped.empty() && ! grouped.front().folder.empty())
            {
                folder = juce::String::fromUTF8 (grouped.front().folder.c_str());
                break;
            }
        }

        if (folder.isEmpty())
        {
            folder = description.manufacturerName.trim();
            if (folder.isEmpty())
            {
                loosePlugins.push_back (description);
                continue;
            }
        }

        auto it = std::find_if (groups.begin(), groups.end(), [&folder] (const auto& group)
        {
            return group.name.compareIgnoreCase (folder) == 0;
        });
        if (it == groups.end())
        {
            groups.push_back ({ folder, {} });
            it = std::prev (groups.end());
        }
        it->plugins.push_back (description);
    }

    const auto byPluginName = [] (const auto& a, const auto& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    };
    std::sort (loosePlugins.begin(), loosePlugins.end(), byPluginName);
    std::sort (groups.begin(), groups.end(), [] (const auto& a, const auto& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    int totalPluginCount = static_cast<int> (loosePlugins.size());
    for (const auto& group : groups)
        totalPluginCount += static_cast<int> (group.plugins.size());

    auto root = std::make_unique<TreeItem> (*this, "VST3", totalPluginCount);
    auto* rootPointer = root.get();
    for (const auto& description : loosePlugins)
        root->addSubItem (new TreeItem (*this, description));

    for (auto& group : groups)
    {
        std::sort (group.plugins.begin(), group.plugins.end(), byPluginName);
        auto folder = std::make_unique<TreeItem> (*this, group.name, static_cast<int> (group.plugins.size()));
        auto* folderPointer = folder.get();
        for (const auto& description : group.plugins)
            folder->addSubItem (new TreeItem (*this, description));
        root->addSubItem (folder.release());
        if (searching)
            folderPointer->setOpen (true);
    }

    tree->setRootItem (root.release());
    tree->setRootItemVisible (true);
    if (searching)
    {
        rootPointer->setOpen (true);
    }
    else if (wasSearching && opennessBeforeSearch != nullptr)
    {
        tree->restoreOpennessState (*opennessBeforeSearch, false);
        opennessBeforeSearch.reset();
    }
    else if (auto savedState = juce::XmlDocument::parse (treeStateFile()))
    {
        tree->restoreOpennessState (*savedState, false);
    }
    else
    {
        rootPointer->setOpen (true);
    }

    rebuildingTree = false;
    tree->repaint();
}

bool BrowserPanel::loadSelectedPlugin()
{
    auto* item = dynamic_cast<TreeItem*> (tree->getSelectedItem (0));
    if (item == nullptr || ! item->isPlugin())
        return false;
    load (item->plugin());
    return true;
}

void BrowserPanel::load (const juce::PluginDescription& description)
{
    if (onLoadRequested)
        onLoadRequested (description);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    g.setColour (theme::border());
    g.drawVerticalLine (0, 0.0f, static_cast<float> (getHeight()));
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
    tree->setBounds (area);
}
