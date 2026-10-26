#include "AllocCounter.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "realtone/SamplerEngine.h"

#include <random>

using namespace realtone;
using namespace fixtures;

RT_TEST(engine_does_not_allocate_while_processing)
{
    const auto dir = rt_test::scratchDir("rt_alloc");
    auto inst = loadOrDie(makeInstrumentFiles(dir, true));
    SamplerEngine eng;
    eng.prepare(48000.0, 32);
    EngineParams p;
    p.vibratoDepthCents = 20.0f;
    p.polyphony = 8;  // force voice stealing during the run
    p.humanizeVelocity = 0.5f;
    p.monoLegato = false;
    eng.setParams(p);
    eng.setInstrument(inst.get());

    std::vector<float> l(512), r(512);
    std::vector<EngineEvent> events(64);
    std::mt19937 rng(11);

    long totalAllocs = 0;
    for (int block = 0; block < 600; ++block)
    {
        std::size_t n = 0;
        for (int e = 0; e < 6; ++e)
        {
            EngineEvent ev;
            ev.offset = static_cast<int>(rng() % 512);
            switch (rng() % 6)
            {
                case 0: case 1: ev.type = EngineEvent::Type::NoteOn; ev.data1 = 36 + static_cast<int>(rng() % 60); ev.data2 = 1 + static_cast<int>(rng() % 127); break;
                case 2: case 3: ev.type = EngineEvent::Type::NoteOff; ev.data1 = 36 + static_cast<int>(rng() % 60); break;
                case 4: ev.type = EngineEvent::Type::PitchBend; ev.data2 = static_cast<int>(rng() % 16384); break;
                default: ev.type = EngineEvent::Type::Controller; ev.data1 = (rng() % 2) ? 64 : 1; ev.data2 = static_cast<int>(rng() % 128); break;
            }
            events[n++] = ev;
        }
        std::sort(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(n), [](const EngineEvent& a, const EngineEvent& b) { return a.offset < b.offset; });

        alloc_counter::begin();
        eng.setParams(p);
        eng.process(events.data(), static_cast<int>(n), l.data(), r.data(), 512);
        totalAllocs += alloc_counter::end();
    }
    CHECK_MSG(totalAllocs == 0, totalAllocs << " heap allocations on the audio path");
}
