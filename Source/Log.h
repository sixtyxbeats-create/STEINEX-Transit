#pragma once
#include <juce_core/juce_core.h>

// Tiny crash-hunting log: Documents/STEINEX/TRANSIT FX/log.txt
namespace steinex
{
inline void logLine (const juce::String& s)
{
    try
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("STEINEX").getChildFile ("TRANSIT FX").getChildFile ("log.txt");
        f.getParentDirectory().createDirectory();
        if (f.getSize() > 200000) f.deleteFile();
        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true) + "  " + s + "\n");
    }
    catch (...) {}
}
}

namespace steinex
{
// Windows only: observes crashes (access violation, stack overflow...) and writes the
// faulting module and offset to log.txt. It never changes how the host handles the crash.
void installCrashLogger();
void removeCrashLogger();
}

#define STX_ONCE() do { static bool stxDone = false; if (! stxDone) { stxDone = true; steinex::logLine (juce::String ("first: ") + __FUNCTION__); } } while (0)
