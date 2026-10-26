#include <cctype>
#include "realtone/Library.h"

#include "realtone/Sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;
using nlohmann::json;

namespace realtone {

bool isSafeRelativePath(const std::string& p)
{
    if (p.empty() || p.size() > 400)
        return false;
    if (p[0] == '/' || p[0] == '\\' || p.find('\\') != std::string::npos || p.find(':') != std::string::npos)
        return false;
    if (p.find('\0') != std::string::npos)
        return false;
    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/'))
        if (part.empty() || part == "." || part == "..")
            return false;
    return true;
}

std::string urlEncodePath(const std::string& path)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
            out.push_back(static_cast<char>(c));
        else
        {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

std::string hostOf(const std::string& url)
{
    const auto scheme = url.find("://");
    if (scheme == std::string::npos)
        return {};
    auto start = scheme + 3;
    auto end = url.find_first_of("/:?#", start);
    std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return host;
}

const char* toString(InstallState s)
{
    switch (s)
    {
        case InstallState::NotInstalled: return "Not installed";
        case InstallState::Incomplete: return "Incomplete";
        case InstallState::Damaged: return "Damaged";
        case InstallState::Installed: return "Installed";
    }
    return "";
}

std::uint64_t LibraryPack::bytesFor(const PackInstrument& inst) const
{
    std::uint64_t total = 0;
    for (const auto& f : inst.files)
    {
        const auto it = files.find(f);
        if (it != files.end())
            total += it->second.size;
    }
    return total;
}

namespace {

std::string str(const json& j, const char* key, const std::string& def = {})
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : def;
}

bool boolean(const json& j, const char* key, bool def)
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : def;
}

bool isHex64(const std::string& s)
{
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

}  // namespace

ManifestParseResult parsePackManifest(const std::string& text)
{
    ManifestParseResult r;
    try
    {
        const json j = json::parse(text);
        if (!j.is_object() || j.value("schema", 0) != 1)
        {
            r.error = "Unsupported library manifest version.";
            return r;
        }
        LibraryPack& p = r.pack;
        p.id = str(j, "id");
        p.name = str(j, "name");
        p.version = str(j, "version");
        if (p.id.empty() || p.name.empty() || !isSafeRelativePath(p.id) || p.id.find('/') != std::string::npos)
        {
            r.error = "The library manifest has a missing or unsafe id/name.";
            return r;
        }
        if (j.contains("license") && j["license"].is_object())
        {
            const auto& l = j["license"];
            p.license.name = str(l, "name");
            p.license.url = str(l, "url");
            p.license.attribution = str(l, "attribution");
            p.license.attributionRequired = boolean(l, "attributionRequired", false);
            p.license.redistributionAllowed = boolean(l, "redistributionAllowed", true);
            p.license.commercialUseAllowed = boolean(l, "commercialUseAllowed", true);
        }
        if (p.license.name.empty())
        {
            r.error = "The library manifest does not state a license.";
            return r;
        }
        if (j.contains("source") && j["source"].is_object())
        {
            const auto& s = j["source"];
            p.homepage = str(s, "homepage");
            p.baseUrl = str(s, "baseUrl");
            if (s.contains("allowedHosts") && s["allowedHosts"].is_array())
                for (const auto& h : s["allowedHosts"])
                    if (h.is_string())
                        p.allowedHosts.push_back(h.get<std::string>());
        }
        if (!p.baseUrl.empty() && p.baseUrl.rfind("https://", 0) != 0)
        {
            r.error = "Library downloads must use https.";
            return r;
        }

        if (j.contains("files") && j["files"].is_array())
        {
            for (const auto& f : j["files"])
            {
                PackFile pf;
                pf.path = str(f, "path");
                pf.size = f.value("size", static_cast<std::uint64_t>(0));
                pf.sha256 = str(f, "sha256");
                if (!isSafeRelativePath(pf.path) || !isHex64(pf.sha256))
                {
                    r.error = "The library manifest lists an invalid file entry: " + pf.path;
                    return r;
                }
                p.files[pf.path] = pf;
            }
        }
        if (j.contains("instruments") && j["instruments"].is_array())
        {
            for (const auto& ji : j["instruments"])
            {
                PackInstrument pi;
                pi.id = str(ji, "id");
                pi.name = str(ji, "name");
                pi.category = str(ji, "category", "Other");
                pi.family = str(ji, "family", "generic");
                pi.description = str(ji, "description");
                pi.sfz = str(ji, "sfz");
                if (ji.contains("tags") && ji["tags"].is_array())
                    for (const auto& t : ji["tags"])
                        if (t.is_string())
                            pi.tags.push_back(t.get<std::string>());
                if (ji.contains("files") && ji["files"].is_array())
                    for (const auto& f : ji["files"])
                        if (f.is_string())
                            pi.files.push_back(f.get<std::string>());
                if (pi.id.empty() || pi.name.empty() || !isSafeRelativePath(pi.sfz))
                {
                    r.error = "The library manifest has an incomplete instrument entry.";
                    return r;
                }
                for (const auto& f : pi.files)
                    if (p.files.count(f) == 0)
                    {
                        r.error = "Instrument " + pi.id + " references an unlisted file: " + f;
                        return r;
                    }
                p.instruments.push_back(std::move(pi));
            }
        }
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.error = std::string("Cannot read library manifest: ") + e.what();
    }
    return r;
}

std::string packToJson(const LibraryPack& p)
{
    json j;
    j["schema"] = 1;
    j["id"] = p.id;
    j["name"] = p.name;
    j["version"] = p.version;
    j["license"] = {{"name", p.license.name},
                    {"url", p.license.url},
                    {"attribution", p.license.attribution},
                    {"attributionRequired", p.license.attributionRequired},
                    {"redistributionAllowed", p.license.redistributionAllowed},
                    {"commercialUseAllowed", p.license.commercialUseAllowed}};
    j["source"] = {{"homepage", p.homepage}, {"baseUrl", p.baseUrl}, {"allowedHosts", p.allowedHosts}};
    j["files"] = json::array();
    for (const auto& [path, f] : p.files)
        j["files"].push_back({{"path", f.path}, {"size", f.size}, {"sha256", f.sha256}});
    j["instruments"] = json::array();
    for (const auto& i : p.instruments)
        j["instruments"].push_back({{"id", i.id}, {"name", i.name}, {"category", i.category}, {"family", i.family},
                                    {"description", i.description}, {"sfz", i.sfz}, {"tags", i.tags}, {"files", i.files}});
    return j.dump(2);
}

// --------------------------------------------------------------------------- catalog

std::vector<std::string> LibraryCatalog::loadManifests(const fs::path& directory)
{
    std::vector<std::string> problems;
    std::error_code ec;
    if (!fs::is_directory(directory, ec))
    {
        problems.push_back("Library manifest folder not found: " + directory.string());
        return problems;
    }
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(directory, ec))
        if (e.is_regular_file() && e.path().extension() == ".json")
            files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (const auto& f : files)
    {
        std::ifstream in(f);
        std::stringstream ss;
        ss << in.rdbuf();
        auto parsed = parsePackManifest(ss.str());
        if (!parsed.ok)
            problems.push_back(f.filename().string() + ": " + parsed.error);
        else
            addPack(parsed.pack);
    }
    return problems;
}

void LibraryCatalog::addPack(const LibraryPack& pack)
{
    packs_.erase(std::remove_if(packs_.begin(), packs_.end(), [&](const LibraryPack& p) { return p.id == pack.id; }), packs_.end());
    packs_.push_back(pack);
}

void LibraryCatalog::addExternal(const std::string& id, const std::string& name, const std::string& category,
                                 const std::string& family, const fs::path& sfzPath)
{
    InstrumentEntry e;
    e.id = id;
    e.name = name;
    e.category = category;
    e.family = family;
    e.packId = "external";
    e.packName = "Imported";
    e.license.name = "User-supplied (not verified by RealTone Studio)";
    e.sfzPath = sfzPath;
    e.rootDir = sfzPath.parent_path();
    e.external = true;
    externals_.push_back(std::move(e));
}

void LibraryCatalog::clearExternals() { externals_.clear(); }

InstrumentEntry LibraryCatalog::evaluate(const LibraryPack& pack, const PackInstrument& inst, bool fullHash,
                                         const std::atomic<bool>* cancel) const
{
    InstrumentEntry e;
    e.id = inst.id;
    e.name = inst.name;
    e.category = inst.category;
    e.family = inst.family;
    e.description = inst.description;
    e.packId = pack.id;
    e.packName = pack.name;
    e.license = pack.license;
    e.tags = inst.tags;
    e.rootDir = root_ / pack.id;
    e.sfzPath = e.rootDir / inst.sfz;
    e.downloadBytes = pack.bytesFor(inst);

    int present = 0;
    std::error_code ec;
    for (const auto& rel : inst.files)
    {
        const auto& pf = pack.files.at(rel);
        const auto path = e.rootDir / rel;
        if (!fs::is_regular_file(path, ec))
        {
            ++e.missingFiles;
            continue;
        }
        ++present;
        if (fs::file_size(path, ec) != pf.size)
        {
            ++e.damagedFiles;
            continue;
        }
        if (fullHash)
        {
            if (cancel != nullptr && cancel->load())
                break;
            if (sha256File(path) != pf.sha256)
                ++e.damagedFiles;
        }
    }
    if (present == 0)
        e.state = InstallState::NotInstalled;
    else if (e.damagedFiles > 0)
        e.state = InstallState::Damaged;
    else if (e.missingFiles > 0)
        e.state = InstallState::Incomplete;
    else
        e.state = InstallState::Installed;
    return e;
}

void LibraryCatalog::refresh()
{
    entries_.clear();
    for (const auto& pack : packs_)
        for (const auto& inst : pack.instruments)
            entries_.push_back(evaluate(pack, inst, false, nullptr));

    std::error_code ec;
    for (auto e : externals_)
    {
        e.state = fs::is_regular_file(e.sfzPath, ec) ? InstallState::Installed : InstallState::NotInstalled;
        e.missingFiles = e.state == InstallState::Installed ? 0 : 1;
        entries_.push_back(std::move(e));
    }
}

InstrumentEntry LibraryCatalog::verifyInstrument(const std::string& id, const std::atomic<bool>* cancel) const
{
    for (const auto& pack : packs_)
        for (const auto& inst : pack.instruments)
            if (inst.id == id)
                return evaluate(pack, inst, true, cancel);
    InstrumentEntry none;
    none.id = id;
    return none;
}

const InstrumentEntry* LibraryCatalog::find(const std::string& id) const
{
    for (const auto& e : entries_)
        if (e.id == id)
            return &e;
    return nullptr;
}

const LibraryPack* LibraryCatalog::findPack(const std::string& packId) const
{
    for (const auto& p : packs_)
        if (p.id == packId)
            return &p;
    return nullptr;
}

// --------------------------------------------------------------------------- installer

InstallResult installInstrument(const LibraryPack& pack, const PackInstrument& instrument, const fs::path& libraryRoot,
                                IFileFetcher& fetcher, const std::function<void(const InstallProgress&)>& progress,
                                const std::atomic<bool>* cancel)
{
    InstallResult result;
    auto fail = [&](const std::string& m) {
        result.error = m;
        return result;
    };

    if (pack.baseUrl.empty())
        return fail("This library has no download source.");
    if (!pack.license.redistributionAllowed)
        return fail("The license of this library does not allow automatic download.");
    if (pack.allowedHosts.empty())
        return fail("The library manifest does not name an authorised download host.");
    const std::string host = hostOf(pack.baseUrl);
    if (std::find(pack.allowedHosts.begin(), pack.allowedHosts.end(), host) == pack.allowedHosts.end())
        return fail("The download host '" + host + "' is not on the library's authorised host list.");

    const fs::path packDir = libraryRoot / pack.id;
    std::error_code ec;
    fs::create_directories(packDir, ec);
    if (ec)
        return fail("Cannot create the library folder: " + packDir.string() + " (" + ec.message() + ")");

    InstallProgress prog;
    prog.filesTotal = static_cast<int>(instrument.files.size());
    std::uint64_t needed = 0;
    for (const auto& rel : instrument.files)
    {
        const auto it = pack.files.find(rel);
        if (it == pack.files.end() || !isSafeRelativePath(rel))
            return fail("The manifest lists an invalid file: " + rel);
        prog.bytesTotal += it->second.size;
        std::error_code e2;
        if (!fs::is_regular_file(packDir / rel, e2) || fs::file_size(packDir / rel, e2) != it->second.size)
            needed += it->second.size;
    }

    const auto space = fs::space(packDir, ec);
    if (!ec && space.available < needed + (64ULL << 20))
        return fail("Not enough free disk space. " + std::to_string(needed >> 20) + " MB are needed.");

    for (const auto& rel : instrument.files)
    {
        if (cancel != nullptr && cancel->load())
        {
            result.cancelled = true;
            result.error = "Installation was cancelled.";
            return result;
        }
        const PackFile& pf = pack.files.at(rel);
        const fs::path dest = packDir / rel;
        prog.currentFile = rel;

        if (fs::is_regular_file(dest, ec) && fs::file_size(dest, ec) == pf.size && sha256File(dest) == pf.sha256)
        {
            ++result.filesAlreadyPresent;
            prog.bytesDone += pf.size;
            ++prog.filesDone;
            if (progress) progress(prog);
            continue;
        }

        fs::create_directories(dest.parent_path(), ec);
        fs::path part = dest;
        part += ".part";
        const std::string url = pack.baseUrl + urlEncodePath(rel);

        bool good = false;
        std::string lastError;
        for (int attempt = 0; attempt < 3 && !good; ++attempt)
        {
            fs::remove(part, ec);
            const std::uint64_t base = prog.bytesDone;
            auto fr = fetcher.fetch(url, part, [&](std::uint64_t got) {
                prog.bytesDone = base + std::min<std::uint64_t>(got, pf.size);
                if (progress) progress(prog);
            }, cancel);
            if (fr.cancelled)
            {
                fs::remove(part, ec);
                result.cancelled = true;
                result.error = "Installation was cancelled.";
                return result;
            }
            if (!fr.ok)
            {
                lastError = fr.error;
                continue;
            }
            if (fs::file_size(part, ec) != pf.size)
            {
                lastError = "downloaded size does not match the manifest";
                continue;
            }
            if (sha256File(part) != pf.sha256)
            {
                lastError = "checksum mismatch";
                continue;
            }
            good = true;
        }
        if (!good)
        {
            fs::remove(part, ec);
            return fail("Could not download " + rel + ": " + lastError);
        }
        fs::remove(dest, ec);
        fs::rename(part, dest, ec);
        if (ec)
            return fail("Could not store " + rel + ": " + ec.message());
        ++result.filesDownloaded;
        prog.bytesDone = std::min(prog.bytesTotal, prog.bytesDone);
        ++prog.filesDone;
        if (progress) progress(prog);
    }

    // Keep the manifest next to the data so license and attribution travel with the files.
    {
        std::ofstream out(packDir / "pack.json", std::ios::trunc);
        out << packToJson(pack);
    }
    result.ok = true;
    return result;
}

RemoveResult removeInstrument(const LibraryPack& pack, const PackInstrument& instrument, const fs::path& libraryRoot)
{
    RemoveResult r;
    const fs::path packDir = libraryRoot / pack.id;
    std::error_code ec;
    if (!fs::is_directory(packDir, ec))
    {
        r.ok = true;  // nothing to remove
        return r;
    }

    // Files still needed by other instruments of this pack are kept, as are their
    // installed state: removing one instrument never breaks another.
    std::set<std::string> keep;
    for (const auto& other : pack.instruments)
    {
        if (other.id == instrument.id)
            continue;
        bool installed = fs::is_regular_file(packDir / other.sfz, ec);
        if (!installed)
            continue;
        keep.insert(other.files.begin(), other.files.end());
    }

    for (const auto& rel : instrument.files)
    {
        if (!isSafeRelativePath(rel) || keep.count(rel) != 0 || pack.files.count(rel) == 0)
            continue;
        const fs::path target = packDir / rel;
        if (fs::is_regular_file(target, ec) && fs::remove(target, ec))
            ++r.filesRemoved;
        fs::path part = target;
        part += ".part";
        fs::remove(part, ec);
        // Prune empty folders, stopping at the pack directory.
        for (auto dir = target.parent_path(); dir != packDir && dir.string().size() > packDir.string().size(); dir = dir.parent_path())
        {
            if (!fs::is_empty(dir, ec) || ec)
                break;
            fs::remove(dir, ec);
        }
    }

    // If nothing of this pack remains, drop its manifest copy and (empty) folder.
    bool anyLeft = false;
    for (const auto& entry : fs::recursive_directory_iterator(packDir, ec))
        if (entry.is_regular_file() && entry.path().filename() != "pack.json")
        {
            anyLeft = true;
            break;
        }
    if (!anyLeft)
    {
        fs::remove(packDir / "pack.json", ec);
        fs::remove(packDir, ec);
    }
    r.ok = true;
    return r;
}

}  // namespace realtone
