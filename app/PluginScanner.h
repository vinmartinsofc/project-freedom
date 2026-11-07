#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <memory>

// Detecta plugins VST3 nas pastas padrão do sistema, em segundo plano, e guarda a lista
// em disco para a próxima abertura ser instantânea. Plugins que travam o scan são
// colocados numa lista de bloqueio automaticamente (arquivo "dead man's pedal").
class PluginScanner : private juce::Thread
{
public:
    explicit PluginScanner (juce::AudioPluginFormatManager&);
    ~PluginScanner() override;

    juce::KnownPluginList& list() { return knownList; }
    void startScan();
    bool isScanning() const { return isThreadRunning(); }

    std::function<void (const juce::String& currentPlugin)> onProgress;   // thread principal
    std::function<void()> onFinished;                                     // thread principal

private:
    void run() override;
    static juce::File dataDirectory();
    void saveList();

    juce::AudioPluginFormatManager& formatManager;
    juce::KnownPluginList knownList;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};
