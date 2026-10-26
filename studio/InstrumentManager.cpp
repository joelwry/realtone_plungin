#include "InstrumentManager.h"

#include "JuceFetcher.h"
#include "Paths.h"
#include "Settings.h"

#include "realtone/Instrument.h"
#include "realtone/InstrumentProfile.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <map>
#include <mutex>

namespace realtone::studio
{

namespace
{

// Samples are large, so every plugin instance in the process shares them.
struct SharedCache
{
    std::mutex mutex;
    std::map<std::string, std::weak_ptr<realtone::Instrument>> items;

    static SharedCache& get()
    {
        static SharedCache c;
        return c;
    }
};

std::string cacheKey(const realtone::InstrumentEntry& e)
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(e.sfzPath, ec);
    return e.sfzPath.string() + "|" + std::to_string(ec ? 0 : t.time_since_epoch().count());
}

}  // namespace

struct InstrumentManager::Impl
{
    juce::ThreadPool pool{juce::ThreadPoolOptions().withNumberOfThreads(2).withThreadName("RealTone loader")};
    std::atomic<int> generation{0};
    std::shared_ptr<std::atomic<bool>> loadCancel = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool> jobCancel{false};
};

InstrumentManager::InstrumentManager() : impl_(std::make_unique<Impl>())
{
    catalog_.setLibraryRoot(std::filesystem::path(Settings::get().libraryRoot().getFullPathName().toStdString()));
    for (const auto& text : bundledManifestTexts())
    {
        auto parsed = realtone::parsePackManifest(text.toStdString());
        if (parsed.ok)
            catalog_.addPack(parsed.pack);
        else
            logMessage("Bundled library manifest rejected: " + juce::String(parsed.error));
    }
    loadExternalsFromSettings();
    catalog_.refresh();
}

InstrumentManager::~InstrumentManager()
{
    impl_->loadCancel->store(true);
    impl_->jobCancel.store(true);
    impl_->generation.fetch_add(1);
    impl_->pool.removeAllJobs(true, 8000);
}

void InstrumentManager::refreshCatalog()
{
    catalog_.refresh();
    sendChangeMessage();
}

void InstrumentManager::setLibraryRoot(const juce::File& dir)
{
    Settings::get().setLibraryRoot(dir);
    catalog_.setLibraryRoot(std::filesystem::path(dir.getFullPathName().toStdString()));
    catalog_.refresh();
    const auto id = status_.instrumentId;
    sendChangeMessage();
    if (id.isNotEmpty())
        selectInstrument(id);
}

void InstrumentManager::setStatus(LoadStatus s)
{
    status_ = std::move(s);
    sendChangeMessage();
}

std::shared_ptr<const realtone::Instrument> InstrumentManager::acquire(const realtone::InstrumentEntry& entry, std::string& error,
                                                                       const std::function<void(float, const std::string&)>& progress,
                                                                       const std::atomic<bool>* cancel)
{
    auto& cache = SharedCache::get();
    const auto key = cacheKey(entry);
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        if (auto it = cache.items.find(key); it != cache.items.end())
            if (auto sp = it->second.lock())
                return sp;
    }

    realtone::LoadOptions opts;
    opts.maxBytes = static_cast<std::uint64_t>(Settings::get().sampleMemoryLimitMB()) * 1024ULL * 1024ULL;
    opts.progress = progress;
    opts.cancel = cancel;
    auto result = realtone::loadInstrument(entry.sfzPath, entry.rootDir, opts);
    if (!result.ok)
    {
        error = result.error;
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        for (auto it = cache.items.begin(); it != cache.items.end();)
            it = it->second.expired() ? cache.items.erase(it) : std::next(it);
        cache.items[key] = result.instrument;
    }
    return result.instrument;
}

void InstrumentManager::selectInstrument(const juce::String& id)
{
    impl_->loadCancel->store(true);                    // abandon any load still running
    impl_->loadCancel = std::make_shared<std::atomic<bool>>(false);
    const int generation = impl_->generation.fetch_add(1) + 1;

    const auto* entry = find(id);
    LoadStatus s;
    s.instrumentId = id;
    if (entry == nullptr)
    {
        s.state = LoadStatus::State::Failed;
        s.message = "This instrument is not in the catalog.";
        current_.reset();
        swap_.post(nullptr);
        setStatus(s);
        return;
    }

    std::error_code ec;
    const bool sfzPresent = std::filesystem::is_regular_file(entry->sfzPath, ec);
    if (!sfzPresent)
    {
        s.state = LoadStatus::State::NotInstalled;
        s.message = entry->external ? "The SFZ file was moved or deleted: " + juce::String(entry->sfzPath.string())
                                    : "Samples for " + juce::String(entry->name.c_str()) + " are not installed (" +
                                          juce::String(static_cast<double>(entry->downloadBytes) / (1024.0 * 1024.0), 0) + " MB download).";
        current_.reset();
        swap_.post(nullptr);  // silence, never another instrument
        setStatus(s);
        return;
    }

    s.state = LoadStatus::State::Loading;
    s.message = "Loading " + juce::String(entry->name.c_str()) + "...";
    setStatus(s);

    const auto entryCopy = *entry;
    auto cancelFlag = impl_->loadCancel;
    Weak weak(this);

    impl_->pool.addJob([this, weak, entryCopy, generation, cancelFlag]() {
        std::string error;
        auto progress = [weak, generation](float f, const std::string& what) {
            juce::MessageManager::callAsync([weak, generation, f, what]() {
                if (weak == nullptr || weak->impl_->generation.load() != generation)
                    return;
                weak->status_.progress = f;
                weak->status_.message = juce::String(what);
                weak->sendChangeMessage();
            });
        };
        auto inst = acquire(entryCopy, error, progress, cancelFlag.get());

        juce::MessageManager::callAsync([weak, generation, entryCopy, inst, error]() {
            if (weak == nullptr || weak->impl_->generation.load() != generation)
                return;  // a newer selection replaced this one
            auto& self = *weak;
            LoadStatus done;
            done.instrumentId = juce::String(entryCopy.id);
            if (inst == nullptr)
            {
                done.state = LoadStatus::State::Failed;
                done.message = error.empty() ? "The instrument could not be loaded." : juce::String(error);
                self.current_.reset();
                self.swap_.post(nullptr);
                logMessage("Load failed for " + done.instrumentId + ": " + done.message);
            }
            else
            {
                self.current_ = inst;
                self.swap_.post(inst);
                done.progress = 1.0f;
                done.missingSamples = static_cast<int>(inst->report.missingSamples.size() + inst->report.failedSamples.size());
                if (done.missingSamples > 0)
                {
                    done.state = LoadStatus::State::ReadyPartial;
                    done.message = juce::String(done.missingSamples) + " sample file(s) are missing or unreadable. Some notes will be silent. Reinstall the instrument in the Library tab.";
                }
                else
                {
                    done.state = LoadStatus::State::Ready;
                    done.message = juce::String(entryCopy.name.c_str()) + " is ready (" +
                                   juce::String(static_cast<double>(inst->memoryBytes()) / (1024.0 * 1024.0), 0) + " MB).";
                }
            }
            self.setStatus(done);
        });
    });
}

// ------------------------------------------------------------------------- library jobs

void InstrumentManager::startJob(const juce::String& title,
                                 std::function<void(const std::function<void(const JobStatus&)>&, JobStatus&, const std::atomic<bool>&)> work,
                                 std::function<void()> onDone)
{
    if (job_.running)
        return;
    impl_->jobCancel.store(false);
    job_ = {};
    job_.running = true;
    job_.title = title;
    sendChangeMessage();

    Weak weak(this);
    impl_->pool.addJob([this, weak, title, work = std::move(work), onDone = std::move(onDone)]() {
        JobStatus local;
        local.running = true;
        local.title = title;
        auto publish = [weak](const JobStatus& s) {
            juce::MessageManager::callAsync([weak, s]() {
                if (weak != nullptr && weak->job_.running)
                {
                    weak->job_ = s;
                    weak->job_.running = true;
                    weak->sendChangeMessage();
                }
            });
        };
        work(publish, local, impl_->jobCancel);
        local.running = false;
        juce::MessageManager::callAsync([weak, local, onDone]() {
            if (weak == nullptr)
                return;
            weak->job_ = local;
            if (onDone)
                onDone();
            weak->catalog_.refresh();
            weak->sendChangeMessage();
        });
    });
}

void InstrumentManager::cancelJob() { impl_->jobCancel.store(true); }

void InstrumentManager::install(const juce::String& instrumentId)
{
    const auto* entry = find(instrumentId);
    if (entry == nullptr || entry->external)
        return;
    const auto* pack = catalog_.findPack(entry->packId);
    if (pack == nullptr)
        return;
    const realtone::LibraryPack packCopy = *pack;
    const std::string id = instrumentId.toStdString();
    const auto root = catalog_.libraryRoot();
    const juce::String name(entry->name.c_str());

    startJob("Installing " + name,
             [packCopy, id, root, name](const std::function<void(const JobStatus&)>& publish, JobStatus& js, const std::atomic<bool>& cancel) {
                 const auto it = std::find_if(packCopy.instruments.begin(), packCopy.instruments.end(),
                                              [&](const realtone::PackInstrument& p) { return p.id == id; });
                 if (it == packCopy.instruments.end())
                 {
                     js.error = "Instrument not found in the library manifest.";
                     return;
                 }
                 JuceFetcher fetcher;
                 auto last = juce::Time::getMillisecondCounter();
                 auto res = realtone::installInstrument(
                     packCopy, *it, root, fetcher,
                     [&](const realtone::InstallProgress& p) {
                         js.progress = p.bytesTotal > 0 ? static_cast<float>(static_cast<double>(p.bytesDone) / static_cast<double>(p.bytesTotal)) : 0.0f;
                         js.message = juce::String(p.filesDone) + " of " + juce::String(p.filesTotal) + " files  -  " +
                                      juce::String(static_cast<double>(p.bytesDone) / (1024.0 * 1024.0), 1) + " of " +
                                      juce::String(static_cast<double>(p.bytesTotal) / (1024.0 * 1024.0), 1) + " MB";
                         const auto now = juce::Time::getMillisecondCounter();
                         if (now - last > 150)
                         {
                             last = now;
                             publish(js);
                         }
                     },
                     &cancel);
                 if (!res.ok)
                 {
                     js.error = juce::String(res.error);
                     logMessage("Install of " + juce::String(id) + " failed: " + js.error);
                 }
                 else
                     js.message = name + " installed.";
                 js.progress = 1.0f;
             },
             [this, instrumentId]() {
                 catalog_.refresh();
                 if (status_.instrumentId == instrumentId)
                     selectInstrument(instrumentId);
             });
}

void InstrumentManager::remove(const juce::String& instrumentId)
{
    const auto* entry = find(instrumentId);
    if (entry == nullptr)
        return;
    if (entry->external)
    {
        removeExternal(instrumentId);
        return;
    }
    const auto* pack = catalog_.findPack(entry->packId);
    if (pack == nullptr)
        return;

    // Let go of the samples first so nothing is playing from files we are about to delete.
    if (status_.instrumentId == instrumentId)
    {
        impl_->loadCancel->store(true);
        current_.reset();
        swap_.post(nullptr);
    }

    const realtone::LibraryPack packCopy = *pack;
    const std::string id = instrumentId.toStdString();
    const auto root = catalog_.libraryRoot();
    const juce::String name(entry->name.c_str());
    startJob("Removing " + name,
             [packCopy, id, root, name](const std::function<void(const JobStatus&)>&, JobStatus& js, const std::atomic<bool>&) {
                 for (const auto& inst : packCopy.instruments)
                     if (inst.id == id)
                     {
                         auto r = realtone::removeInstrument(packCopy, inst, root);
                         js.message = r.ok ? name + " removed (" + juce::String(r.filesRemoved) + " files)." : juce::String();
                         if (!r.ok)
                             js.error = juce::String(r.error);
                     }
                 js.progress = 1.0f;
             },
             [this, instrumentId]() {
                 catalog_.refresh();
                 if (status_.instrumentId == instrumentId)
                     selectInstrument(instrumentId);  // reports "not installed"
             });
}

void InstrumentManager::verify(const juce::String& instrumentId)
{
    const auto* entry = find(instrumentId);
    if (entry == nullptr || entry->external)
        return;
    const auto catalogCopy = catalog_;
    Weak weakSelf(this);
    const std::string id = instrumentId.toStdString();
    const juce::String name(entry->name.c_str());
    startJob("Verifying " + name,
             [weakSelf, catalogCopy, id, name](const std::function<void(const JobStatus&)>&, JobStatus& js, const std::atomic<bool>& cancel) {
                 auto e = catalogCopy.verifyInstrument(id, &cancel);
                 juce::String report = name + ": " + juce::String(realtone::toString(e.state));
                 if (e.missingFiles > 0) report << ", " << e.missingFiles << " file(s) missing";
                 if (e.damagedFiles > 0) report << ", " << e.damagedFiles << " file(s) damaged";
                 if (e.state == realtone::InstallState::Installed) report << ". All checksums match.";
                 js.message = report;
                 js.progress = 1.0f;
                 const auto text = report;
                 juce::MessageManager::callAsync([weakSelf, text]() { if (weakSelf != nullptr) weakSelf->verifyReport_ = text; });
             },
             nullptr);
}

// -------------------------------------------------------------------------- external SFZ

void InstrumentManager::loadExternalsFromSettings()
{
    externals_.clear();
    catalog_.clearExternals();
    for (const auto& line : Settings::get().getList(keys::externalInstruments))
    {
        const auto parts = juce::StringArray::fromTokens(line, "|", "");
        if (parts.size() != 5)
            continue;
        externals_.push_back({parts[0], parts[1], parts[2], parts[3], parts[4]});
        catalog_.addExternal(parts[0].toStdString(), parts[1].toStdString(), parts[2].toStdString(), parts[3].toStdString(),
                             std::filesystem::path(parts[4].toStdString()));
    }
}

void InstrumentManager::saveExternalsToSettings()
{
    juce::StringArray lines;
    for (const auto& e : externals_)
        lines.add(e.id + "|" + e.name + "|" + e.category + "|" + e.family + "|" + e.path);
    Settings::get().setList(keys::externalInstruments, lines);
}

juce::String InstrumentManager::addExternalSfz(const juce::File& sfz, const juce::String& name, const juce::String& category,
                                               const juce::String& family)
{
    const auto id = "external." + juce::String::toHexString(sfz.getFullPathName().hashCode64());
    externals_.erase(std::remove_if(externals_.begin(), externals_.end(), [&](const External& e) { return e.id == id; }), externals_.end());
    externals_.push_back({id, name.replaceCharacter('|', ' '), category.replaceCharacter('|', ' '), family, sfz.getFullPathName()});
    saveExternalsToSettings();
    loadExternalsFromSettings();
    catalog_.refresh();
    sendChangeMessage();
    return id;
}

void InstrumentManager::removeExternal(const juce::String& id)
{
    externals_.erase(std::remove_if(externals_.begin(), externals_.end(), [&](const External& e) { return e.id == id; }), externals_.end());
    saveExternalsToSettings();
    loadExternalsFromSettings();
    catalog_.refresh();
    if (status_.instrumentId == id)
    {
        current_.reset();
        swap_.post(nullptr);
        setStatus({});
    }
    sendChangeMessage();
}

}  // namespace realtone::studio
