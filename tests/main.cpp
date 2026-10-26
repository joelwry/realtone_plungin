#include "TestSupport.h"

#include <cstring>
#include <iostream>

int main(int argc, char** argv)
{
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
    return failed == 0 ? 0 : 1;
}
