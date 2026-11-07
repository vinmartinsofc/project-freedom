#include "PluginScanner.h"

juce::File PluginScanner::dataDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ProjectFreedom");
    dir.createDirectory();
    return dir;
}

PluginScanner::PluginScanner (juce::AudioPluginFormatManager& fm)
    : juce::Thread ("Plugin scanner"), formatManager (fm)
{
    if (auto xml = juce::XmlDocument::parse (dataDirectory().getChildFile ("plugins.xml")))
        knownList.recreateFromXml (*xml);
}

PluginScanner::~PluginScanner()
{
    *alive = false;
    stopThread (8000);
}

void PluginScanner::startScan()
{
    if (! isThreadRunning())
        startThread();
}

void PluginScanner::saveList()
{
    if (auto xml = knownList.createXml())
        xml->writeTo (dataDirectory().getChildFile ("plugins.xml"));
}

void PluginScanner::run()
{
    const auto deadMans = dataDirectory().getChildFile ("scan-crash-guard.txt");
    auto flag = alive;

    for (int i = 0; i < formatManager.getNumFormats() && ! threadShouldExit(); ++i)
    {
        auto* format = formatManager.getFormat (i);
        juce::PluginDirectoryScanner scanner (knownList, *format, format->getDefaultLocationsToSearch(),
                                              true, deadMans, false);
        juce::String name;
        while (! threadShouldExit() && scanner.scanNextFile (true, name))
        {
            juce::MessageManager::callAsync ([this, flag, name]
            {
                if (*flag && onProgress) onProgress (name);
            });
        }
    }

    saveList();
    juce::MessageManager::callAsync ([this, flag]
    {
        if (*flag && onFinished) onFinished();
    });
}
