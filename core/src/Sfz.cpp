#include "realtone/Sfz.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace realtone {

namespace {

using OpMap = std::map<std::string, std::string>;

std::string trim(const std::string& s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string stripComments(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '/' && i + 1 < in.size() && in[i + 1] == '/')
        {
            while (i < in.size() && in[i] != '\n')
                ++i;
            if (i < in.size())
                out.push_back('\n');
        }
        else if (in[i] == '/' && i + 1 < in.size() && in[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < in.size() && !(in[i] == '*' && in[i + 1] == '/'))
                ++i;
            ++i;
            out.push_back(' ');
        }
        else
            out.push_back(in[i]);
    }
    return out;
}

bool isInside(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const auto r = root.lexically_normal();
    const auto c = candidate.lexically_normal();
    auto ri = r.begin();
    auto ci = c.begin();
    for (; ri != r.end(); ++ri, ++ci)
    {
        if (ri->empty())
            continue;  // trailing separator
        if (ci == c.end() || *ci != *ri)
            return false;
    }
    return true;
}

struct Preprocessor
{
    std::map<std::string, std::string> defines;
    std::filesystem::path rootDir;
    std::vector<std::string> warnings;
    int depth = 0;

    std::string run(const std::string& text, const std::filesystem::path& baseDir)
    {
        std::istringstream in(stripComments(text));
        std::string line, out;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto t = trim(line);
            if (t.rfind("#define", 0) == 0)
            {
                std::istringstream ls(t.substr(7));
                std::string name, value;
                ls >> name;
                std::getline(ls, value);
                if (!name.empty())
                    defines[name] = trim(value);
                continue;
            }
            if (t.rfind("#include", 0) == 0)
            {
                const auto q1 = t.find('"');
                const auto q2 = t.rfind('"');
                if (q1 == std::string::npos || q2 <= q1)
                    continue;
                std::string rel = t.substr(q1 + 1, q2 - q1 - 1);
                std::replace(rel.begin(), rel.end(), '\\', '/');
                const auto target = (baseDir / rel).lexically_normal();
                if (depth >= 8 || !isInside(rootDir, target))
                {
                    warnings.push_back("Ignored #include outside the library: " + rel);
                    continue;
                }
                std::ifstream f(target);
                if (!f)
                {
                    warnings.push_back("Missing #include file: " + rel);
                    continue;
                }
                std::stringstream ss;
                ss << f.rdbuf();
                ++depth;
                out += run(ss.str(), target.parent_path());
                --depth;
                continue;
            }
            // Longest names first so $VOL2 is not clobbered by $VOL.
            std::string expanded = line;
            std::vector<std::pair<std::string, std::string>> sorted(defines.begin(), defines.end());
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
            for (const auto& [name, value] : sorted)
            {
                for (std::size_t pos = expanded.find(name); pos != std::string::npos; pos = expanded.find(name, pos + value.size()))
                    expanded.replace(pos, name.size(), value);
            }
            out += expanded;
            out.push_back('\n');
        }
        return out;
    }
};

// Splits one line into headers and key=value pairs. A value runs until the next
// " identifier=" or "<" on the same line, so sample names may contain spaces.
struct Token
{
    bool isHeader = false;
    std::string name, value;
};

void tokenizeLine(const std::string& line, std::vector<Token>& tokens)
{
    std::size_t i = 0;
    const auto n = line.size();
    while (i < n)
    {
        if (std::isspace(static_cast<unsigned char>(line[i])))
        {
            ++i;
            continue;
        }
        if (line[i] == '<')
        {
            const auto close = line.find('>', i);
            if (close == std::string::npos)
                return;
            tokens.push_back({true, lower(line.substr(i + 1, close - i - 1)), {}});
            i = close + 1;
            continue;
        }
        if (isIdentStart(line[i]))
        {
            std::size_t j = i;
            while (j < n && isIdentChar(line[j]))
                ++j;
            if (j < n && line[j] == '=')
            {
                const std::string name = lower(line.substr(i, j - i));
                std::size_t vStart = j + 1;
                std::size_t vEnd = vStart;
                // Find the end of the value.
                while (vEnd < n)
                {
                    if (line[vEnd] == '<')
                        break;
                    if (std::isspace(static_cast<unsigned char>(line[vEnd])))
                    {
                        std::size_t k = vEnd;
                        while (k < n && std::isspace(static_cast<unsigned char>(line[k])))
                            ++k;
                        if (k < n && line[k] == '<')
                            break;
                        if (k < n && isIdentStart(line[k]))
                        {
                            std::size_t m = k;
                            while (m < n && isIdentChar(line[m]))
                                ++m;
                            if (m < n && line[m] == '=')
                                break;
                        }
                    }
                    ++vEnd;
                }
                tokens.push_back({false, name, trim(line.substr(vStart, vEnd - vStart))});
                i = vEnd;
                continue;
            }
            i = j;  // stray word, skip
            continue;
        }
        ++i;
    }
}

bool parseFloat(const std::string& s, float& out)
{
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str())
        return false;
    out = v;
    return true;
}

bool parseInt(const std::string& s, int& out)
{
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str())
        return false;
    out = static_cast<int>(v);
    return true;
}

bool parseKey(const std::string& s, int& out)
{
    const int k = parseNoteToken(trim(s));
    if (k < 0)
        return false;
    out = k;
    return true;
}

const std::set<std::string>& ignoredButKnown()
{
    static const std::set<std::string> s = {"ampeg_dynamic", "group_label", "default_path", "set_cc",
                                            "label_cc",      "hint_",       "sw_label",     "region_label"};
    return s;
}

struct Builder
{
    std::filesystem::path rootDir;
    std::filesystem::path baseDir;
    SfzParseResult result;
    std::string defaultPath;
    bool inControl = false;

    OpMap global, master, group, region;
    bool haveRegion = false;

    void applyOp(SfzRegion& r, const std::string& name, const std::string& value)
    {
        float f;
        int i;
        bool ok = true;
        if (name == "sample")
        {
            std::string p = value;
            std::replace(p.begin(), p.end(), '\\', '/');
            std::string dp = defaultPath;
            std::replace(dp.begin(), dp.end(), '\\', '/');
            if (!dp.empty() && dp.back() != '/')
                dp.push_back('/');
            r.sample = dp + p;
        }
        else if (name == "lokey") ok = parseKey(value, r.lokey);
        else if (name == "hikey") ok = parseKey(value, r.hikey);
        else if (name == "key")
        {
            ok = parseKey(value, i);
            if (ok)
                r.lokey = r.hikey = r.pitchKeycenter = i;
        }
        else if (name == "pitch_keycenter") ok = parseKey(value, r.pitchKeycenter);
        else if (name == "pitch_keytrack") { ok = parseFloat(value, f); if (ok) r.pitchKeytrack = f; }
        else if (name == "lovel") ok = parseInt(value, r.lovel);
        else if (name == "hivel") ok = parseInt(value, r.hivel);
        else if (name == "lorand") { ok = parseFloat(value, f); if (ok) r.lorand = f; }
        else if (name == "hirand") { ok = parseFloat(value, f); if (ok) r.hirand = f; }
        else if (name == "seq_length") ok = parseInt(value, r.seqLength);
        else if (name == "seq_position") ok = parseInt(value, r.seqPosition);
        else if (name == "trigger")
        {
            const auto v = lower(value);
            if (v == "attack") r.trigger = SfzRegion::Trigger::Attack;
            else if (v == "release") r.trigger = SfzRegion::Trigger::Release;
            else if (v == "first") r.trigger = SfzRegion::Trigger::First;
            else if (v == "legato") r.trigger = SfzRegion::Trigger::Legato;
            else ok = false;
        }
        else if (name == "loop_mode" || name == "loopmode")
        {
            const auto v = lower(value);
            if (v == "no_loop") r.loopMode = SfzRegion::LoopMode::NoLoop;
            else if (v == "one_shot") r.loopMode = SfzRegion::LoopMode::OneShot;
            else if (v == "loop_continuous") r.loopMode = SfzRegion::LoopMode::LoopContinuous;
            else if (v == "loop_sustain") r.loopMode = SfzRegion::LoopMode::LoopSustain;
            else ok = false;
        }
        else if (name == "loop_start" || name == "loopstart") { ok = parseFloat(value, f); if (ok) r.loopStart = f; }
        else if (name == "loop_end" || name == "loopend") { ok = parseFloat(value, f); if (ok) r.loopEnd = f; }
        else if (name == "offset") { ok = parseFloat(value, f); if (ok) r.offset = f; }
        else if (name == "end") { ok = parseFloat(value, f); if (ok) r.end = f; }
        else if (name == "volume") { ok = parseFloat(value, f); if (ok) r.volumeDb = f; }
        else if (name == "pan") { ok = parseFloat(value, f); if (ok) r.pan = std::clamp(f, -100.0f, 100.0f); }
        else if (name == "tune") { ok = parseFloat(value, f); if (ok) r.tuneCents += f; }
        else if (name == "transpose") { ok = parseFloat(value, f); if (ok) r.tuneCents += f * 100.0f; }
        else if (name == "amp_veltrack") { ok = parseFloat(value, f); if (ok) r.ampVeltrack = f; }
        else if (name == "ampeg_attack") { ok = parseFloat(value, f); if (ok) r.ampegAttack = std::max(0.0f, f); }
        else if (name == "ampeg_hold") { ok = parseFloat(value, f); if (ok) r.ampegHold = std::max(0.0f, f); }
        else if (name == "ampeg_decay") { ok = parseFloat(value, f); if (ok) r.ampegDecay = std::max(0.0f, f); }
        else if (name == "ampeg_sustain") { ok = parseFloat(value, f); if (ok) r.ampegSustain = std::clamp(f, 0.0f, 100.0f); }
        else if (name == "ampeg_release") { ok = parseFloat(value, f); if (ok) r.ampegRelease = std::max(0.0f, f); }
        else if (name == "group") ok = parseInt(value, r.group);
        else if (name == "off_by") ok = parseInt(value, r.offBy);
        else if (ignoredButKnown().count(name) != 0) {}
        else
        {
            result.unsupportedOpcodes.insert(name);
            return;
        }
        if (!ok)
            result.warnings.push_back("Invalid value '" + value + "' for opcode " + name);
    }

    void flushRegion()
    {
        if (!haveRegion)
            return;
        haveRegion = false;

        SfzRegion r;
        for (const OpMap* level : {&global, &master, &group, &region})
            for (const auto& [name, value] : *level)
                applyOp(r, name, value);

        if (r.sample.empty() || r.sample.rfind('*', 0) == 0)
        {
            if (r.sample.empty())
                result.warnings.push_back("Skipped a region with no sample.");
            else
                result.warnings.push_back("Skipped generator region '" + r.sample + "' (not supported).");
            return;
        }
        const auto resolved = (baseDir / r.sample).lexically_normal();
        if (!isInside(rootDir, resolved))
        {
            result.warnings.push_back("Rejected sample path outside the library: " + r.sample);
            return;
        }
        r.sample = std::filesystem::path(r.sample).lexically_normal().generic_string();
        r.lokey = std::clamp(r.lokey, 0, 127);
        r.hikey = std::clamp(r.hikey, 0, 127);
        r.lovel = std::clamp(r.lovel, 0, 127);
        r.hivel = std::clamp(r.hivel, 0, 127);
        r.seqLength = std::max(1, r.seqLength);
        r.seqPosition = std::clamp(r.seqPosition, 1, r.seqLength);
        if (r.lokey > r.hikey || r.lovel > r.hivel)
        {
            result.warnings.push_back("Skipped a region with an empty key or velocity range: " + r.sample);
            return;
        }
        result.regions.push_back(std::move(r));
    }

    void header(const std::string& name)
    {
        inControl = false;
        if (name == "control")
        {
            flushRegion();
            inControl = true;
        }
        else if (name == "global")
        {
            flushRegion();
            global.clear(); master.clear(); group.clear(); region.clear();
        }
        else if (name == "master")
        {
            flushRegion();
            master.clear(); group.clear(); region.clear();
        }
        else if (name == "group")
        {
            flushRegion();
            group.clear(); region.clear();
        }
        else if (name == "region")
        {
            flushRegion();
            region.clear();
            haveRegion = true;
        }
        else
        {
            flushRegion();
            result.warnings.push_back("Unsupported header <" + name + "> ignored.");
        }
        currentHeader = name;
    }

    void opcode(const std::string& name, const std::string& value)
    {
        if (inControl)
        {
            if (name == "default_path")
                defaultPath = value;
            return;
        }
        if (currentHeader == "global") global[name] = value;
        else if (currentHeader == "master") master[name] = value;
        else if (currentHeader == "group") group[name] = value;
        else if (currentHeader == "region") region[name] = value;
    }

    std::string currentHeader;
};

}  // namespace

int parseNoteToken(const std::string& tokenIn)
{
    const std::string token = lower(trim(tokenIn));
    if (token.empty())
        return -1;
    if (std::isdigit(static_cast<unsigned char>(token[0])) || token[0] == '-')
    {
        char* end = nullptr;
        const long v = std::strtol(token.c_str(), &end, 10);
        if (*end != '\0' || v < 0 || v > 127)
            return -1;
        return static_cast<int>(v);
    }
    static const std::map<char, int> base = {{'c', 0}, {'d', 2}, {'e', 4}, {'f', 5}, {'g', 7}, {'a', 9}, {'b', 11}};
    const auto it = base.find(token[0]);
    if (it == base.end())
        return -1;
    int semis = it->second;
    std::size_t pos = 1;
    if (pos < token.size() && (token[pos] == '#' || token[pos] == 'b'))
    {
        semis += token[pos] == '#' ? 1 : -1;
        ++pos;
    }
    if (pos >= token.size())
        return -1;
    char* end = nullptr;
    const long octave = std::strtol(token.c_str() + pos, &end, 10);
    if (*end != '\0')
        return -1;
    const long midi = (octave + 1) * 12 + semis;
    return (midi < 0 || midi > 127) ? -1 : static_cast<int>(midi);
}

SfzParseResult parseSfz(const std::string& text, const std::filesystem::path& baseDir,
                        const std::filesystem::path& rootDir)
{
    Preprocessor pre;
    pre.rootDir = rootDir;
    const std::string cleaned = pre.run(text, baseDir);

    Builder b;
    b.rootDir = rootDir;
    b.baseDir = baseDir;

    std::istringstream in(cleaned);
    std::string line;
    std::vector<Token> tokens;
    while (std::getline(in, line))
    {
        tokens.clear();
        tokenizeLine(line, tokens);
        for (const auto& t : tokens)
        {
            if (t.isHeader)
                b.header(t.name);
            else
                b.opcode(t.name, t.value);
        }
    }
    b.flushRegion();

    b.result.warnings.insert(b.result.warnings.begin(), pre.warnings.begin(), pre.warnings.end());
    if (b.result.regions.empty())
    {
        b.result.error = "The SFZ file defines no playable regions.";
        return std::move(b.result);
    }
    b.result.ok = true;
    return std::move(b.result);
}

SfzParseResult parseSfzFile(const std::filesystem::path& path, const std::filesystem::path& rootDir)
{
    std::ifstream f(path);
    if (!f)
    {
        SfzParseResult r;
        r.error = "Cannot open instrument definition: " + path.string();
        return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parseSfz(ss.str(), path.parent_path(), rootDir);
}

}  // namespace realtone
