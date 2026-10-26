#pragma once

#include "realtone/Instrument.h"

#include <array>
#include <atomic>
#include <memory>

namespace realtone {

// Hands a freshly loaded instrument from a UI/worker thread to the audio thread without
// locks or allocation on the audio side, and sends the replaced instrument back so it is
// destroyed off the audio thread.
//
//   message thread:  post(inst);  ...  collectGarbage();   (e.g. from a timer)
//   audio thread:    if (swap.poll(ptr)) engine.setInstrument(ptr);
class InstrumentSwap
{
public:
    InstrumentSwap() = default;
    ~InstrumentSwap();
    InstrumentSwap(const InstrumentSwap&) = delete;
    InstrumentSwap& operator=(const InstrumentSwap&) = delete;

    // Any non-audio thread. A null instrument means "play nothing".
    void post(std::shared_ptr<const Instrument> instrument);
    void collectGarbage();

    // Audio thread only. Returns true when a new instrument became current; `out` is
    // then the instrument to use (possibly null). The pointer stays valid until the
    // next successful poll().
    bool poll(const Instrument*& out) noexcept;

    const Instrument* current() const noexcept { return current_ != nullptr ? current_->instrument.get() : nullptr; }

private:
    struct Holder
    {
        std::shared_ptr<const Instrument> instrument;
    };

    static constexpr int kRetiredSlots = 16;

    std::atomic<Holder*> pending_{nullptr};
    Holder* current_ = nullptr;  // audio thread only
    std::array<std::atomic<Holder*>, kRetiredSlots> retired_{};
};

}  // namespace realtone
