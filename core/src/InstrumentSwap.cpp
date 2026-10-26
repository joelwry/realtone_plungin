#include "realtone/InstrumentSwap.h"

namespace realtone {

InstrumentSwap::~InstrumentSwap()
{
    delete pending_.exchange(nullptr);
    delete current_;
    for (auto& slot : retired_)
        delete slot.exchange(nullptr);
}

void InstrumentSwap::post(std::shared_ptr<const Instrument> instrument)
{
    auto* holder = new Holder{std::move(instrument)};
    // If the previous request was never picked up, the audio thread cannot also hold it:
    // exchange hands ownership to exactly one side.
    delete pending_.exchange(holder, std::memory_order_acq_rel);
}

void InstrumentSwap::collectGarbage()
{
    for (auto& slot : retired_)
        delete slot.exchange(nullptr, std::memory_order_acq_rel);
}

bool InstrumentSwap::poll(const Instrument*& out) noexcept
{
    if (pending_.load(std::memory_order_acquire) == nullptr)
        return false;

    // Make sure the outgoing instrument has somewhere to go before taking the new one,
    // otherwise it would have to be freed on this thread.
    int freeSlot = -1;
    if (current_ != nullptr)
    {
        for (int i = 0; i < kRetiredSlots; ++i)
            if (retired_[static_cast<std::size_t>(i)].load(std::memory_order_acquire) == nullptr)
            {
                freeSlot = i;
                break;
            }
        if (freeSlot < 0)
            return false;  // garbage not collected yet; try again next block
    }

    Holder* incoming = pending_.exchange(nullptr, std::memory_order_acq_rel);
    if (incoming == nullptr)
        return false;

    if (current_ != nullptr)
        retired_[static_cast<std::size_t>(freeSlot)].store(current_, std::memory_order_release);
    current_ = incoming;
    out = current_->instrument.get();
    return true;
}

}  // namespace realtone
