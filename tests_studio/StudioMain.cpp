#include "TestSupport.h"

#include "StudioTestSupport.h"

#include "../ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    realtone::ui::RealToneLookAndFeel laf;
    juce::LookAndFeel::setDefaultLookAndFeel(&laf);

    // Keep the tests away from the real user profile.
    const auto home = rt_test::scratchDir("studio_userdata");
    studio_test::useIsolatedSettings(home);

    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failed = 0;
    for (const auto& c : rt_test::registry())
    {
        if (filter != nullptr && c.name.find(filter) == std::string::npos)
            continue;
        const int before = rt_test::failureCount();
        std::fprintf(stderr, "[ RUN  ] %s\n", c.name.c_str());
        try
        {
            c.fn();
        }
        catch (const std::exception& e)
        {
            rt_test::report("(exception)", 0, e.what());
        }
        ++run;
        if (rt_test::failureCount() != before)
        {
            ++failed;
            std::fprintf(stderr, "[ FAIL ] %s\n", c.name.c_str());
        }
        else
            std::fprintf(stderr, "[  OK  ] %s\n", c.name.c_str());
    }
    std::fprintf(stderr, "\n%d test(s) run, %d failed\n", run, failed);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    return failed == 0 ? 0 : 1;
}
