#pragma once

#include "realtone/InstrumentSwap.h"
#include "realtone/Library.h"

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>

namespace realtone::studio
{

struct LoadStatus
{
    enum class State
    {
        Idle,          // nothing selected
        Loading,
        Ready,
        ReadyPartial,  // plays, but some sample files are missing
        Failed,
        NotInstalled   // selected, but its sample files are not on this computer
    };

    State state = State::Idle;
    juce::String instrumentId;
    juce::String message;
    float progress = 0.0f;
    int missingSamples = 0;
};

struct JobStatus
{
    bool running = false;
    juce::String title;
    juce::String message;
    float progress = 0.0f;
    juce::String error;  // set when the last job failed
};

// Owns the instrument catalog for one plugin instance or the standalone app, loads samples on
// worker threads and hands finished instruments to the audio thread through InstrumentSwap.
// All public methods are for the message thread.
class InstrumentManager : public juce::ChangeBroadcaster
{
public:
    InstrumentManager();
    ~InstrumentManager() override;

    // Catalog ---------------------------------------------------------------------------
    void refreshCatalog();
    const std::vector<realtone::InstrumentEntry>& entries() const { return catalog_.instruments(); }
    const realtone::InstrumentEntry* find(const juce::String& id) const { return catalog_.find(id.toStdString()); }
    const realtone::LibraryCatalog& catalog() const { return catalog_; }
    juce::File libraryRoot() const;
    void setLibraryRoot(const juce::File& dir);

    // Selection -------------------------------------------------------------------------
    // Starts a background load. Never substitutes another instrument if the files are missing.
    void selectInstrument(const juce::String& id);
    LoadStatus status() const { return status_; }
    juce::String selectedId() const { return status_.instrumentId; }
    realtone::InstrumentSwap& swap() { return swap_; }
    std::shared_ptr<const realtone::Instrument> loadedInstrument() const { return current_; }

    // Blocking load for offline work (render, convert). Call from a worker thread. Shares the
    // process-wide cache, so rendering with the instrument that is already loaded is instant.
    std::shared_ptr<const realtone::Instrument> acquire(const realtone::InstrumentEntry& entry, std::string& error,
                                                        const std::function<void(float, const std::string&)>& progress = {},
                                                        const std::atomic<bool>* cancel = nullptr);

    // Library jobs ----------------------------------------------------------------------
    void install(const juce::String& instrumentId);
    void remove(const juce::String& instrumentId);
    void verify(const juce::String& instrumentId);
    void cancelJob();
    JobStatus job() const { return job_; }
    juce::String lastVerifyReport() const { return verifyReport_; }

    // User-supplied SFZ libraries (not verified, not managed) -----------------------------
    juce::String addExternalSfz(const juce::File& sfz, const juce::String& name, const juce::String& category,
                                const juce::String& family);
    void removeExternal(const juce::String& id);

    // Frees instruments the audio thread has finished with. Called from a timer.
    void collectGarbage() { swap_.collectGarbage(); }

private:
    JUCE_DECLARE_WEAK_REFERENCEABLE(InstrumentManager)

    struct Impl;
    using Weak = juce::WeakReference<InstrumentManager>;
    void loadExternalsFromSettings();
    void saveExternalsToSettings();
    // `work` runs on a worker thread and reports through the JobStatus it is given; `onDone` runs
    // on the message thread afterwards.
    void startJob(const juce::String& title,
                  std::function<void(const std::function<void(const JobStatus&)>& publish, JobStatus&, const std::atomic<bool>&)> work,
                  std::function<void()> onDone);
    void setStatus(LoadStatus s);

    realtone::LibraryCatalog catalog_;
    realtone::InstrumentSwap swap_;
    std::shared_ptr<const realtone::Instrument> current_;
    LoadStatus status_;
    JobStatus job_;
    juce::String verifyReport_;
    std::unique_ptr<Impl> impl_;
    struct External { juce::String id, name, category, family, path; };
    std::vector<External> externals_;
};

// The manifests compiled into the program.
std::vector<juce::String> bundledManifestTexts();
juce::String bundledLicenseNotices();

}  // namespace realtone::studio
