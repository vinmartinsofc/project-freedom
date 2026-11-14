#include "AudioFileBrowser.h"
#include <algorithm>
#include <array>
#include <vector>

namespace
{
const std::array<juce::Colour, 7> favoriteColors {
    juce::Colour (0xfff44336), juce::Colour (0xffff9800), juce::Colour (0xffffeb3b),
    juce::Colour (0xff4caf50), juce::Colour (0xff2196f3), juce::Colour (0xff9c27b0),
    juce::Colour (0xff9e9e9e)
};

const std::array<const char*, 7> favoriteColorNames {
    "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo", "Cinza"
};
}

class AudioFileBrowser::FileTreeItem : public juce::TreeViewItem
{
public:
    enum class Kind { FileSystem, ComputerRoot, FavoritesRoot, FavoriteCategory, FavoriteEntry };

    FileTreeItem (AudioFileBrowser& browser, juce::File path, Kind itemKind = Kind::FileSystem, int color = -1)
        : owner (browser), file (std::move (path)), kind (itemKind), favoriteColor (color) {}

    bool mightContainSubItems() override
    {
        return (kind != Kind::FileSystem && kind != Kind::FavoriteEntry) || file.isDirectory();
    }

    juce::String getUniqueName() const override
    {
        if (kind == Kind::ComputerRoot) return "computer";
        if (kind == Kind::FavoritesRoot) return "favorites";
        if (kind == Kind::FavoriteCategory) return "favorite-" + juce::String (favoriteColor);
        return file.getFullPathName();
    }

    const juce::File& getFile() const { return file; }

    void itemSelectionChanged (bool isNowSelected) override
    {
        if (isNowSelected)
            owner.selectFile (kind == Kind::FileSystem || kind == Kind::FavoriteEntry ? file : juce::File());
    }

    void itemClicked (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu()
            && (kind == Kind::FileSystem || kind == Kind::FavoriteEntry)
            && (file.isDirectory() || AudioFileBrowser::isAudioFile (file)))
            owner.showFavoriteMenu (file, event.getScreenPosition());
        else if (! event.mods.isPopupMenu()
                 && (kind == Kind::FileSystem || kind == Kind::FavoriteEntry))
            owner.previewFile (file);
    }

    juce::var getDragSourceDescription() override
    {
        return (kind == Kind::FileSystem || kind == Kind::FavoriteEntry)
                && file.existsAsFile() && AudioFileBrowser::isAudioFile (file)
            ? juce::var ("audio:" + file.getFullPathName()) : juce::var();
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
            g.fillAll (theme::accent().withAlpha (0.25f));

        juce::String label;
        bool isBold = false;
        if (kind == Kind::ComputerRoot) { label = "Este Computador"; isBold = true; }
        else if (kind == Kind::FavoritesRoot) { label = "Favoritos"; isBold = true; }
        else if (kind == Kind::FavoriteCategory)
        {
            label = favoriteColorNames[static_cast<std::size_t> (favoriteColor)];
            isBold = true;
        }
        else
        {
            label = file.getFileName();
            isBold = file.isDirectory();
        }

        const int textX = favoriteColor >= 0 ? 22 : 5;
        if (favoriteColor >= 0)
        {
            g.setColour (favoriteColors[static_cast<std::size_t> (favoriteColor)]);
            g.fillRoundedRectangle (5.0f, static_cast<float> (height / 2 - 5), 10.0f, 10.0f, 2.0f);
        }
        g.setColour (theme::text());
        g.setFont (juce::FontOptions (12.0f, isBold ? juce::Font::bold : juce::Font::plain));
        g.drawText (label, textX, 0, width - textX - 5, height, juce::Justification::centredLeft, true);
    }

    void itemOpennessChanged (bool isOpen) override
    {
        if (isOpen && getNumSubItems() == 0)
            owner.populate (*this);
    }

private:
    AudioFileBrowser& owner;
    juce::File file;
    Kind kind;
    int favoriteColor = -1;
};

bool AudioFileBrowser::FileTreeView::keyPressed (const juce::KeyPress& key)
{
    auto* previouslySelected = getSelectedItem (0);
    const bool handled = juce::TreeView::keyPressed (key);
    if (handled && getSelectedItem (0) != previouslySelected)
        owner.previewFile (owner.selectedFile);
    return handled;
}

AudioFileBrowser::AudioFileBrowser()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "ProjectFreedomAudioFavorites";
    options.filenameSuffix = ".settings";
    options.folderName = "ProjectFreedom";
    options.osxLibrarySubFolder = "Application Support";
    favoriteProperties.setStorageParameters (options);
    favoriteSettings = favoriteProperties.getUserSettings();
    loadFavorites();

    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::text());
    addAndMakeVisible (title);

    tree.setColour (juce::TreeView::backgroundColourId, theme::panel());
    tree.setRootItemVisible (true);
    tree.setRootItem (new FileTreeItem (*this, {}, FileTreeItem::Kind::ComputerRoot));
    tree.getRootItem()->setOpen (true);
    tree.setIndentSize (10);
    tree.setWantsKeyboardFocus (true);
    addAndMakeVisible (tree);

    status.setFont (juce::FontOptions (10.5f));
    status.setColour (juce::Label::textColourId, theme::textDim());
    addAndMakeVisible (status);

    previewButton.onClick = [this]
    {
        if (selectedFile.existsAsFile() && onPreviewRequested)
        {
            setStatus ("Carregando preview...");
            onPreviewRequested (selectedFile);
        }
    };
    stopButton.onClick = [this]
    {
        if (onPreviewStopRequested)
            onPreviewStopRequested();
        setStatus ("Preview parado");
    };
    autoPreviewButton.setClickingTogglesState (true);
    autoPreviewButton.setToggleState (true, juce::dontSendNotification);
    autoPreviewButton.setTooltip ("Reproduzir automaticamente ao selecionar um arquivo");
    previewGain.setSliderStyle (juce::Slider::LinearHorizontal);
    previewGain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    previewGain.setRange (0.0, 1.0, 0.01);
    previewGain.setValue (0.5, juce::dontSendNotification);
    previewGain.setTooltip ("Volume da pre-escuta");
    previewGainLabel.setColour (juce::Label::textColourId, theme::textDim());
    previewGainLabel.setFont (juce::FontOptions (11.0f));
    previewGain.onValueChange = [this]
    {
        if (onPreviewGainChanged)
            onPreviewGainChanged (static_cast<float> (previewGain.getValue()));
    };
    previewButton.setEnabled (false);
    addAndMakeVisible (previewButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (autoPreviewButton);
    addAndMakeVisible (previewGainLabel);
    addAndMakeVisible (previewGain);
}

void AudioFileBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    g.setColour (theme::border());
    g.drawVerticalLine (0, 0.0f, static_cast<float> (getHeight()));
}

void AudioFileBrowser::resized()
{
    auto area = getLocalBounds().reduced (8);
    title.setBounds (area.removeFromTop (28));
    auto controls = area.removeFromTop (28);
    autoPreviewButton.setBounds (controls.removeFromLeft (56));
    controls.removeFromLeft (2);
    previewButton.setBounds (controls.removeFromLeft (58));
    controls.removeFromLeft (2);
    stopButton.setBounds (controls.removeFromLeft (56));
    auto volume = area.removeFromTop (22);
    previewGainLabel.setBounds (volume.removeFromLeft (48));
    previewGain.setBounds (volume);
    area.removeFromTop (4);
    tree.setBounds (area.removeFromTop (std::max (0, area.getHeight() - 30)));
    status.setBounds (area);
}

void AudioFileBrowser::setFavoritesMode (bool showing)
{
    if (showingFavorites == showing)
        return;

    showingFavorites = showing;
    selectFile ({});
    title.setText (showingFavorites ? "Favoritos" : "Arquivos de audio", juce::dontSendNotification);
    tree.setRootItem (new FileTreeItem (*this, {}, showingFavorites
        ? FileTreeItem::Kind::FavoritesRoot : FileTreeItem::Kind::ComputerRoot));
    tree.getRootItem()->setOpen (true);
}

bool AudioFileBrowser::isAudioFile (const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".wave" || extension == ".aif"
        || extension == ".aiff" || extension == ".flac" || extension == ".ogg";
}

void AudioFileBrowser::selectFile (const juce::File& file)
{
    selectedFile = file.existsAsFile() && isAudioFile (file) ? file : juce::File();
    previewButton.setEnabled (selectedFile.existsAsFile());
    if (onPreviewStopRequested)
        onPreviewStopRequested();
    setStatus (selectedFile.existsAsFile()
                   ? "Selecionado: " + selectedFile.getFileName()
                   : "Selecione um arquivo de audio para ouvir");
}

void AudioFileBrowser::previewFile (const juce::File& file)
{
    if (autoPreviewButton.getToggleState() && file.existsAsFile() && isAudioFile (file))
    {
        selectedFile = file;
        previewButton.setEnabled (true);
        setStatus ("Carregando preview...");
        if (onPreviewRequested)
            onPreviewRequested (file);
    }
}

void AudioFileBrowser::showFavoriteMenu (const juce::File& file, juce::Point<int> screenPosition)
{
    const auto existing = std::find_if (favorites.begin(), favorites.end(),
        [&file] (const Favorite& favorite) { return favorite.file == file; });

    juce::PopupMenu colors;
    for (int color = 0; color < static_cast<int> (favoriteColorNames.size()); ++color)
        colors.addItem (color + 1, favoriteColorNames[static_cast<std::size_t> (color)]);

    juce::PopupMenu menu;
    menu.addSubMenu ("Adicionar aos favoritos", colors);
    if (existing != favorites.end())
        menu.addItem (20, "Remover dos favoritos");

    juce::Component::SafePointer<AudioFileBrowser> safeThis (this);
    const juce::PopupMenu::Options options;
    menu.showMenuAsync (options.withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
        [safeThis, file] (int result)
        {
            if (safeThis == nullptr)
                return;
            if (result >= 1 && result <= 7)
                safeThis->setFavorite (file, result - 1);
            else if (result == 20)
                safeThis->removeFavorite (file);
        });
}

void AudioFileBrowser::setFavorite (const juce::File& file, int color)
{
    if (! file.exists() || (! file.isDirectory() && ! isAudioFile (file))
        || color < 0 || color >= static_cast<int> (favoriteColors.size()))
        return;

    const auto existing = std::find_if (favorites.begin(), favorites.end(),
        [&file] (const Favorite& favorite) { return favorite.file == file; });
    if (existing == favorites.end())
        favorites.push_back ({ file, color });
    else
        existing->color = color;

    saveFavorites();
    if (showingFavorites)
    {
        setFavoritesMode (false);
        setFavoritesMode (true);
    }
}

void AudioFileBrowser::removeFavorite (const juce::File& file)
{
    favorites.erase (std::remove_if (favorites.begin(), favorites.end(),
        [&file] (const Favorite& favorite) { return favorite.file == file; }), favorites.end());
    saveFavorites();
    if (showingFavorites)
    {
        setFavoritesMode (false);
        setFavoritesMode (true);
    }
}

void AudioFileBrowser::loadFavorites()
{
    if (favoriteSettings == nullptr)
        return;

    const auto xml = juce::parseXML (favoriteSettings->getValue ("audioFavorites"));
    if (xml == nullptr)
        return;

    const auto root = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < root.getNumChildren(); ++i)
    {
        const auto item = root.getChild (i);
        const juce::File file (item.getProperty ("path").toString());
        const int color = static_cast<int> (item.getProperty ("color", 0));
        if (file.exists() && (file.isDirectory() || isAudioFile (file))
            && color >= 0 && color < static_cast<int> (favoriteColors.size()))
            favorites.push_back ({ file, color });
    }
}

void AudioFileBrowser::saveFavorites()
{
    if (favoriteSettings == nullptr)
        return;

    juce::ValueTree root ("Favorites");
    for (const auto& favorite : favorites)
    {
        juce::ValueTree item ("Item");
        item.setProperty ("path", favorite.file.getFullPathName(), nullptr);
        item.setProperty ("color", favorite.color, nullptr);
        root.appendChild (item, nullptr);
    }
    favoriteSettings->setValue ("audioFavorites", root.toXmlString());
    favoriteSettings->saveIfNeeded();
}

void AudioFileBrowser::populate (FileTreeItem& item)
{
    if (item.getUniqueName() == "favorites")
    {
        for (int color = 0; color < static_cast<int> (favoriteColorNames.size()); ++color)
            item.addSubItem (new FileTreeItem (*this, {}, FileTreeItem::Kind::FavoriteCategory, color));
        return;
    }

    if (item.getUniqueName().startsWith ("favorite-"))
    {
        const int color = item.getUniqueName().substring (9).getIntValue();
        std::vector<juce::File> entries;
        for (const auto& favorite : favorites)
            if (favorite.color == color)
                entries.push_back (favorite.file);
        std::sort (entries.begin(), entries.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
        });
        for (const auto& file : entries)
            item.addSubItem (new FileTreeItem (*this, file, FileTreeItem::Kind::FavoriteEntry, color));
        return;
    }

    if (item.getUniqueName() == "computer")
    {
        juce::Array<juce::File> roots;
        juce::File::findFileSystemRoots (roots);
        for (const auto& root : roots)
            item.addSubItem (new FileTreeItem (*this, root));
        return;
    }

    juce::Array<juce::File> children;
    item.getFile().findChildFiles (children, juce::File::findFilesAndDirectories, false);
    std::vector<juce::File> directories, audioFiles;
    for (const auto& child : children)
    {
        if (child.isDirectory())
            directories.push_back (child);
        else if (isAudioFile (child))
            audioFiles.push_back (child);
    }
    const auto sortByName = [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    };
    std::sort (directories.begin(), directories.end(), sortByName);
    std::sort (audioFiles.begin(), audioFiles.end(), sortByName);
    for (const auto& directory : directories)
        item.addSubItem (new FileTreeItem (*this, directory));
    for (const auto& file : audioFiles)
        item.addSubItem (new FileTreeItem (*this, file));
}
