#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>
#include <vector>

class AudioFileBrowser : public juce::Component
{
public:
    AudioFileBrowser();
    std::function<void (const juce::File&)> onPreviewRequested;
    std::function<void()> onPreviewStopRequested;
    std::function<void (float)> onPreviewGainChanged;
    void setFavoritesMode (bool);
    void setStatus (const juce::String& text) { status.setText (text, juce::dontSendNotification); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Favorite
    {
        juce::File file;
        int color = 0;
    };

    class FileTreeItem;
    class FileTreeView : public juce::TreeView
    {
    public:
        explicit FileTreeView (AudioFileBrowser& browser) : owner (browser) {}
        bool keyPressed (const juce::KeyPress&) override;

    private:
        AudioFileBrowser& owner;
    };

    void populate (FileTreeItem&);
    void selectFile (const juce::File&);
    void previewFile (const juce::File&);
    void showFavoriteMenu (const juce::File&, juce::Point<int> screenPosition);
    void setFavorite (const juce::File&, int color);
    void removeFavorite (const juce::File&);
    void loadFavorites();
    void saveFavorites();
    static bool isAudioFile (const juce::File&);

    juce::Label title { {}, "Arquivos de audio" };
    juce::Label status { {}, "Selecione um arquivo de audio para ouvir" };
    FileTreeView tree { *this };
    juce::TextButton previewButton { "Ouvir" }, stopButton { "Parar" };
    juce::ToggleButton autoPreviewButton { "Auto" };
    juce::Label previewGainLabel { {}, "Volume" };
    juce::Slider previewGain;
    juce::File selectedFile;
    juce::ApplicationProperties favoriteProperties;
    juce::PropertiesFile* favoriteSettings = nullptr;
    std::vector<Favorite> favorites;
    bool showingFavorites = false;
};
