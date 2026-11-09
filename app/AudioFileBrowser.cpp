#include "AudioFileBrowser.h"
#include <algorithm>
#include <vector>

class AudioFileBrowser::FileTreeItem : public juce::TreeViewItem
{
public:
    FileTreeItem (AudioFileBrowser& browser, juce::File path, bool computer = false)
        : owner (browser), file (std::move (path)), computerRoot (computer) {}

    bool mightContainSubItems() override { return computerRoot || file.isDirectory(); }

    juce::String getUniqueName() const override
    {
        return computerRoot ? "computer" : file.getFullPathName();
    }

    const juce::File& getFile() const { return file; }

    void itemSelectionChanged (bool isNowSelected) override
    {
        if (isNowSelected)
            owner.selectFile (file.isDirectory() ? juce::File() : file);
    }

    juce::var getDragSourceDescription() override
    {
        return ! computerRoot && file.existsAsFile() && AudioFileBrowser::isAudioFile (file)
            ? juce::var ("audio:" + file.getFullPathName()) : juce::var();
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
            g.fillAll (theme::accent().withAlpha (0.25f));

        g.setColour (theme::text());
        g.setFont (juce::FontOptions (12.0f, file.isDirectory() || computerRoot ? juce::Font::bold : juce::Font::plain));
        const auto label = computerRoot ? "Este Computador" : file.getFileName();
        g.drawText (label, 5, 0, width - 10, height, juce::Justification::centredLeft, true);
    }

    void itemOpennessChanged (bool isOpen) override
    {
        if (isOpen && getNumSubItems() == 0)
            owner.populate (*this);
    }

private:
    AudioFileBrowser& owner;
    juce::File file;
    bool computerRoot = false;
};

AudioFileBrowser::AudioFileBrowser()
{
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::text());
    addAndMakeVisible (title);

    tree.setColour (juce::TreeView::backgroundColourId, theme::panel());
    tree.setRootItemVisible (true);
    tree.setRootItem (new FileTreeItem (*this, {}, true));
    tree.getRootItem()->setOpen (true);
    tree.setIndentSize (10);
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
    previewButton.setEnabled (false);
    addAndMakeVisible (previewButton);
    addAndMakeVisible (stopButton);
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
    previewButton.setBounds (controls.removeFromLeft (68));
    controls.removeFromLeft (4);
    stopButton.setBounds (controls.removeFromLeft (68));
    area.removeFromTop (4);
    tree.setBounds (area.removeFromTop (std::max (0, area.getHeight() - 30)));
    status.setBounds (area);
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

void AudioFileBrowser::populate (FileTreeItem& item)
{
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
