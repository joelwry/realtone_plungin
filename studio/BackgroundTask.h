#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>

namespace realtone::studio
{

// One long-running job at a time on its own thread (analysis, rendering, export). The work function
// reports progress and polls for cancellation; the finish callback always runs on the message thread.
class BackgroundTask : private juce::Thread
{
public:
    explicit BackgroundTask(const juce::String& name) : juce::Thread(name) {}
    ~BackgroundTask() override
    {
        cancel();
        stopThread(15000);
    }

    using Work = std::function<void(BackgroundTask&)>;

    // Returns false if a job is already running.
    bool start(Work work, std::function<void(bool cancelled)> onFinished)
    {
        if (isThreadRunning())
            return false;
        stopThread(1000);
        work_ = std::move(work);
        finished_ = std::move(onFinished);
        cancelled_ = false;
        progress_ = 0.0f;
        running_ = true;
        setStage({});
        startThread();
        return true;
    }

    void cancel() { cancelled_ = true; signalThreadShouldExit(); }
    bool isRunning() const { return running_.load(); }
    bool cancelRequested() const { return cancelled_.load() || threadShouldExit(); }
    float progress() const { return progress_.load(); }
    juce::String stage() const
    {
        const juce::ScopedLock sl(lock_);
        return stage_;
    }

    void report(float fraction, const juce::String& stage)
    {
        progress_ = fraction;
        setStage(stage);
    }
    const std::atomic<bool>* cancelFlag() const { return &cancelled_; }

private:
    void setStage(const juce::String& s)
    {
        const juce::ScopedLock sl(lock_);
        stage_ = s;
    }

    void run() override
    {
        try
        {
            work_(*this);
        }
        catch (const std::exception&)
        {
            // Work functions report their own errors; an escaped exception must not take the app down.
        }
        const bool wasCancelled = cancelRequested();
        running_ = false;
        auto done = finished_;
        juce::MessageManager::callAsync([done, wasCancelled]() {
            if (done)
                done(wasCancelled);
        });
    }

    Work work_;
    std::function<void(bool)> finished_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> running_{false};
    std::atomic<float> progress_{0.0f};
    juce::CriticalSection lock_;
    juce::String stage_;
};

}  // namespace realtone::studio
