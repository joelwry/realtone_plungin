#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace realtone {

struct LicenseInfo
{
    std::string name;            // e.g. "CC0 1.0 Universal"
    std::string url;
    std::string attribution;     // credit line shown in the About / Library views
    bool attributionRequired = false;
    bool redistributionAllowed = true;
    bool commercialUseAllowed = true;
};

struct PackFile
{
    std::string path;            // relative, '/' separators, never contains ".."
    std::uint64_t size = 0;
    std::string sha256;
};

struct PackInstrument
{
    std::string id;
    std::string name;
    std::string category;
    std::string family;
    std::string description;
    std::string sfz;                       // relative path of the .sfz inside the pack
    std::vector<std::string> tags;
    std::vector<std::string> files;        // every pack file the instrument needs (including the .sfz)
};

struct LibraryPack
{
    std::string id;
    std::string name;
    std::string version;
    LicenseInfo license;
    std::string homepage;
    std::string baseUrl;                   // https only
    std::vector<std::string> allowedHosts; // downloads from any other host are refused
    std::map<std::string, PackFile> files; // by relative path
    std::vector<PackInstrument> instruments;

    std::uint64_t bytesFor(const PackInstrument& inst) const;
};

struct ManifestParseResult
{
    bool ok = false;
    std::string error;
    LibraryPack pack;
};

ManifestParseResult parsePackManifest(const std::string& json);
std::string packToJson(const LibraryPack& pack);

enum class InstallState
{
    NotInstalled,
    Incomplete,   // some files present, some missing
    Damaged,      // present but wrong size or hash
    Installed
};

const char* toString(InstallState s);

struct InstrumentEntry
{
    std::string id;
    std::string name;
    std::string category;
    std::string family;
    std::string description;
    std::string packId;
    std::string packName;
    LicenseInfo license;
    std::vector<std::string> tags;
    std::filesystem::path sfzPath;         // absolute, valid whatever the state
    std::filesystem::path rootDir;         // directory SFZ paths are confined to
    InstallState state = InstallState::NotInstalled;
    int missingFiles = 0;
    int damagedFiles = 0;
    std::uint64_t downloadBytes = 0;
    bool external = false;                 // user-imported SFZ not managed by a pack
};

// Knows which instruments exist (from pack manifests and user imports), where they
// live, and whether the files on disk are complete. Pure bookkeeping, no downloads.
class LibraryCatalog
{
public:
    void setLibraryRoot(const std::filesystem::path& root) { root_ = root; }
    const std::filesystem::path& libraryRoot() const { return root_; }

    // Loads every *.json pack manifest in the directory. Returns problems found.
    std::vector<std::string> loadManifests(const std::filesystem::path& directory);
    void addPack(const LibraryPack& pack);
    void addExternal(const std::string& id, const std::string& name, const std::string& category,
                     const std::string& family, const std::filesystem::path& sfzPath);
    void clearExternals();

    // Cheap check (existence and size). Use verifyInstrument for a full hash check.
    void refresh();
    // Re-hashes every file of one instrument. Blocking; run on a worker thread.
    InstrumentEntry verifyInstrument(const std::string& id, const std::atomic<bool>* cancel = nullptr) const;

    const std::vector<InstrumentEntry>& instruments() const { return entries_; }
    const InstrumentEntry* find(const std::string& id) const;
    const LibraryPack* findPack(const std::string& packId) const;
    const std::vector<LibraryPack>& packs() const { return packs_; }
    std::filesystem::path packDirectory(const std::string& packId) const { return root_ / packId; }

private:
    InstrumentEntry evaluate(const LibraryPack& pack, const PackInstrument& inst, bool fullHash,
                             const std::atomic<bool>* cancel) const;

    std::filesystem::path root_;
    std::vector<LibraryPack> packs_;
    std::vector<InstrumentEntry> externals_;
    std::vector<InstrumentEntry> entries_;
};

struct FetchResult
{
    bool ok = false;
    bool cancelled = false;
    std::string error;
};

// Download transport, supplied by the application so the catalog and installer stay
// free of platform networking code and can be tested against a local directory.
class IFileFetcher
{
public:
    virtual ~IFileFetcher() = default;
    virtual FetchResult fetch(const std::string& url, const std::filesystem::path& destination,
                              const std::function<void(std::uint64_t received)>& onBytes,
                              const std::atomic<bool>* cancel) = 0;
};

struct InstallProgress
{
    std::uint64_t bytesDone = 0;
    std::uint64_t bytesTotal = 0;
    int filesDone = 0;
    int filesTotal = 0;
    std::string currentFile;
};

struct InstallResult
{
    bool ok = false;
    bool cancelled = false;
    std::string error;
    int filesDownloaded = 0;
    int filesAlreadyPresent = 0;
};

InstallResult installInstrument(const LibraryPack& pack, const PackInstrument& instrument,
                                const std::filesystem::path& libraryRoot, IFileFetcher& fetcher,
                                const std::function<void(const InstallProgress&)>& progress = {},
                                const std::atomic<bool>* cancel = nullptr);

struct RemoveResult
{
    bool ok = false;
    std::string error;
    int filesRemoved = 0;
};

// Deletes the instrument's files unless another installed instrument of the same pack
// still uses them. Only ever touches files listed in the pack manifest, inside the pack folder.
RemoveResult removeInstrument(const LibraryPack& pack, const PackInstrument& instrument,
                              const std::filesystem::path& libraryRoot);

bool isSafeRelativePath(const std::string& path);
std::string urlEncodePath(const std::string& path);
std::string hostOf(const std::string& url);

}  // namespace realtone
