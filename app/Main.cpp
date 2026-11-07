#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class FreedomApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    void initialise (const juce::String&) override
    {
        if (auto* laf = dynamic_cast<juce::LookAndFeel_V4*> (&juce::LookAndFeel::getDefaultLookAndFeel()))
            laf->setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
        window = std::make_unique<MainWindow> (getApplicationName());
    }
    void shutdown() override                       { window.reset(); }
    void systemRequestedQuit() override            { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff1e1e1e), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (1280, 760);
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (FreedomApplication)
