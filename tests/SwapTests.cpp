#include "Fixtures.h"
#include "TestSupport.h"

#include "realtone/InstrumentSwap.h"

#include <atomic>
#include <thread>

using namespace realtone;
using namespace fixtures;

RT_TEST(instrument_swap_delivers_latest_and_returns_old_for_disposal)
{
    const auto dir = rt_test::scratchDir("swap1");
    auto a = loadOrDie(makeInstrumentFiles(dir));
    auto b = loadOrDie(makeInstrumentFiles(dir));
    std::weak_ptr<const Instrument> weakA = a;

    InstrumentSwap swap;
    const Instrument* out = nullptr;
    CHECK(!swap.poll(out));

    swap.post(a);
    CHECK(swap.poll(out));
    CHECK(out == a.get());
    CHECK(!swap.poll(out));            // nothing new

    a.reset();
    CHECK(!weakA.expired());           // still alive: the swap holds it for the audio thread
    swap.post(b);
    CHECK(swap.poll(out));
    CHECK(out == b.get());
    CHECK(!weakA.expired());           // retired but not yet destroyed (never on the audio thread)
    swap.collectGarbage();
    CHECK(weakA.expired());

    // A request that was superseded before the audio thread saw it is simply dropped.
    swap.post(nullptr);
    swap.post(b);
    CHECK(swap.poll(out));
    CHECK(out == b.get());
    swap.post(nullptr);
    CHECK(swap.poll(out));
    CHECK(out == nullptr);
}

RT_TEST(instrument_swap_survives_concurrent_posting)
{
    const auto dir = rt_test::scratchDir("swap2");
    auto base = makeInstrumentFiles(dir);
    std::vector<std::weak_ptr<const Instrument>> made;

    InstrumentSwap swap;
    std::atomic<bool> stop{false};
    std::atomic<long> polls{0};
    std::thread audio([&] {
        const Instrument* cur = nullptr;
        while (!stop.load())
        {
            if (swap.poll(cur))
                polls.fetch_add(1);
            if (cur != nullptr)
                (void)cur->regions.size();  // touch it: must never be freed underneath us
        }
    });

    for (int i = 0; i < 60; ++i)
    {
        auto inst = loadOrDie(base);
        made.push_back(inst);
        swap.post(inst);
        if (i % 3 == 0)
            swap.collectGarbage();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop = true;
    audio.join();
    swap.post(nullptr);
    const Instrument* out = nullptr;
    swap.poll(out);
    for (int i = 0; i < 4; ++i)
        swap.collectGarbage();
    CHECK(polls.load() > 5);
    // Everything except what the swap itself still holds must be gone.
    int alive = 0;
    for (auto& w : made)
        if (!w.expired())
            ++alive;
    CHECK_MSG(alive <= 1, alive << " instruments still alive");
}
