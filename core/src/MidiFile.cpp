#include "realtone/MidiFile.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

namespace realtone {

void Sequence::sortEvents()
{
    std::stable_sort(events.begin(), events.end(), [](const SequenceEvent& a, const SequenceEvent& b) {
        if (a.time != b.time)
            return a.time < b.time;
        return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    });
    durationSeconds = events.empty() ? 0.0 : events.back().time;
}

namespace {

struct Reader
{
    const std::uint8_t* p;
    std::size_t size;
    std::size_t pos = 0;

    bool has(std::size_t n) const { return pos + n <= size; }
    std::uint8_t u8() { return p[pos++]; }
    std::uint32_t u16() { auto v = (std::uint32_t(p[pos]) << 8) | p[pos + 1]; pos += 2; return v; }
    std::uint32_t u32() { auto v = (std::uint32_t(p[pos]) << 24) | (std::uint32_t(p[pos + 1]) << 16) | (std::uint32_t(p[pos + 2]) << 8) | p[pos + 3]; pos += 4; return v; }
    bool varLen(std::uint32_t& out)
    {
        out = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (!has(1)) return false;
            const auto b = u8();
            out = (out << 7) | (b & 0x7F);
            if ((b & 0x80) == 0) return true;
        }
        return false;
    }
};

struct RawEvent
{
    std::uint64_t tick = 0;
    SequenceEvent::Kind kind = SequenceEvent::Kind::NoteOn;
    int channel = 0, d1 = 0, d2 = 0, track = 0;
};

struct TempoChange
{
    std::uint64_t tick;
    std::uint32_t microsPerQuarter;
};

bool wantedController(int cc)
{
    return cc == 1 || cc == 7 || cc == 11 || cc == 64 || cc == 120 || cc == 121 || cc == 123;
}

}  // namespace

MidiParseResult parseMidiMemory(const std::uint8_t* data, std::size_t size)
{
    MidiParseResult out;
    auto fail = [&](const std::string& m) {
        out.ok = false;
        out.error = m;
        return out;
    };

    Reader rd{data, size};
    if (size < 14 || !(data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd'))
        return fail("This is not a Standard MIDI File (missing MThd header).");
    rd.pos = 4;
    const auto headerLen = rd.u32();
    if (headerLen < 6 || !rd.has(headerLen))
        return fail("The MIDI header is truncated or damaged.");
    const int format = static_cast<int>(rd.u16());
    const int trackCount = static_cast<int>(rd.u16());
    const std::uint32_t division = rd.u16();
    rd.pos = 8 + headerLen;

    if (format > 1)
        return fail("MIDI format 2 files (independent sequences) are not supported. Re-save the file as format 0 or 1.");
    if (trackCount < 1 || trackCount > 512)
        return fail("The MIDI file reports an unusable number of tracks.");
    if (division == 0)
        return fail("The MIDI file has an invalid time division.");

    const bool smpte = (division & 0x8000) != 0;
    double ticksPerSecond = 0.0;
    std::uint32_t ppq = 0;
    if (smpte)
    {
        const int fps = -static_cast<std::int8_t>((division >> 8) & 0xFF);
        const int tpf = static_cast<int>(division & 0xFF);
        if (fps <= 0 || tpf <= 0)
            return fail("The MIDI file has an invalid SMPTE time division.");
        ticksPerSecond = (fps == 29 ? 29.97 : static_cast<double>(fps)) * tpf;
    }
    else
        ppq = division;

    Sequence& seq = out.sequence;
    seq.format = format;
    std::vector<RawEvent> raw;
    std::vector<TempoChange> tempos;

    int tracksRead = 0;
    while (tracksRead < trackCount && rd.has(8))
    {
        const bool isTrack = data[rd.pos] == 'M' && data[rd.pos + 1] == 'T' && data[rd.pos + 2] == 'r' && data[rd.pos + 3] == 'k';
        rd.pos += 4;
        std::uint32_t len = rd.u32();
        if (!isTrack)
        {
            if (!rd.has(len)) break;
            rd.pos += len;
            continue;  // unknown chunk, skip
        }
        if (!rd.has(len))
        {
            seq.warnings.push_back("Track " + std::to_string(tracksRead) + " is truncated; reading what is available.");
            len = static_cast<std::uint32_t>(size - rd.pos);
        }

        Reader tr{data + rd.pos, len};
        rd.pos += len;

        TrackInfo info;
        info.index = tracksRead;
        std::uint64_t tick = 0;
        int runningStatus = 0;
        bool allPerc = true;
        std::map<int, std::vector<std::uint64_t>> openNotes;  // key = ch*128+note
        std::uint64_t lastTick = 0;

        while (tr.pos < tr.size)
        {
            std::uint32_t delta;
            if (!tr.varLen(delta)) { seq.warnings.push_back("Damaged event timing in track " + std::to_string(tracksRead) + "."); break; }
            tick += delta;
            if (!tr.has(1)) break;

            int status = tr.p[tr.pos];
            if (status < 0x80)
            {
                if (runningStatus == 0) { seq.warnings.push_back("Missing status byte in track " + std::to_string(tracksRead) + "."); break; }
                status = runningStatus;
            }
            else
            {
                ++tr.pos;
                if (status < 0xF0)
                    runningStatus = status;
            }

            if (status == 0xFF)
            {
                if (!tr.has(1)) break;
                const int type = tr.u8();
                std::uint32_t mlen;
                if (!tr.varLen(mlen) || !tr.has(mlen)) { seq.warnings.push_back("Truncated meta event."); break; }
                const std::uint8_t* md = tr.p + tr.pos;
                if (type == 0x51 && mlen == 3)
                    tempos.push_back({tick, (std::uint32_t(md[0]) << 16) | (std::uint32_t(md[1]) << 8) | md[2]});
                else if (type == 0x03 && info.name.empty())
                    info.name.assign(reinterpret_cast<const char*>(md), mlen);
                tr.pos += mlen;
                if (type == 0x2F) break;
                continue;
            }
            if (status == 0xF0 || status == 0xF7)
            {
                std::uint32_t slen;
                if (!tr.varLen(slen) || !tr.has(slen)) break;
                tr.pos += slen;
                continue;
            }

            const int type = status & 0xF0;
            const int ch = status & 0x0F;
            const int needed = (type == 0xC0 || type == 0xD0) ? 1 : 2;
            if (!tr.has(static_cast<std::size_t>(needed))) break;
            const int d1 = tr.u8() & 0x7F;
            const int d2 = needed == 2 ? (tr.u8() & 0x7F) : 0;

            RawEvent e;
            e.tick = tick; e.channel = ch; e.track = tracksRead;
            switch (type)
            {
                case 0x90:
                    if (d2 > 0)
                    {
                        e.kind = SequenceEvent::Kind::NoteOn; e.d1 = d1; e.d2 = d2;
                        openNotes[ch * 128 + d1].push_back(tick);
                        info.noteCount++;
                        info.lowestNote = std::min(info.lowestNote, d1);
                        info.highestNote = std::max(info.highestNote, d1);
                        if (info.firstChannel < 0) info.firstChannel = ch;
                        if (ch != 9) allPerc = false;
                        raw.push_back(e);
                        break;
                    }
                    [[fallthrough]];
                case 0x80:
                {
                    auto& open = openNotes[ch * 128 + d1];
                    if (!open.empty()) open.erase(open.begin());
                    e.kind = SequenceEvent::Kind::NoteOff; e.d1 = d1; e.d2 = 0;
                    raw.push_back(e);
                    break;
                }
                case 0xB0:
                    if (wantedController(d1))
                    {
                        e.kind = SequenceEvent::Kind::Controller; e.d1 = d1; e.d2 = d2;
                        raw.push_back(e);
                    }
                    break;
                case 0xE0:
                    e.kind = SequenceEvent::Kind::PitchBend; e.d1 = 0; e.d2 = (d2 << 7) | d1;
                    raw.push_back(e);
                    break;
                case 0xC0:
                    if (info.program < 0) info.program = d1;
                    break;
                default: break;
            }
            lastTick = tick;
        }

        // Close any notes left hanging at the end of the track.
        for (auto& [key, ticks] : openNotes)
            for (std::size_t i = 0; i < ticks.size(); ++i)
            {
                RawEvent e;
                e.tick = std::max(lastTick, tick); e.kind = SequenceEvent::Kind::NoteOff;
                e.channel = key / 128; e.d1 = key % 128; e.track = tracksRead;
                raw.push_back(e);
                seq.warnings.push_back("A note on track " + std::to_string(tracksRead) + " never ended; it was closed at the end of the track.");
            }

        info.looksLikePercussion = info.noteCount > 0 && allPerc;
        if (info.noteCount == 0) { info.lowestNote = 0; info.highestNote = 0; }
        seq.tracks.push_back(info);
        seq.noteCount += info.noteCount;
        ++tracksRead;
    }

    if (tracksRead == 0)
        return fail("The MIDI file contains no tracks.");
    if (tracksRead < trackCount)
        seq.warnings.push_back("The file declares " + std::to_string(trackCount) + " tracks but only " + std::to_string(tracksRead) + " were readable.");
    if (seq.noteCount == 0)
        return fail("The MIDI file contains no notes.");

    // Tempo map -> seconds.
    std::stable_sort(tempos.begin(), tempos.end(), [](const TempoChange& a, const TempoChange& b) { return a.tick < b.tick; });
    seq.initialTempoBpm = (!tempos.empty() && tempos.front().tick == 0) ? 60000000.0 / tempos.front().microsPerQuarter : 120.0;

    auto toSeconds = [&](std::uint64_t tick) -> double {
        if (smpte)
            return static_cast<double>(tick) / ticksPerSecond;
        double seconds = 0.0;
        std::uint64_t prevTick = 0;
        double micros = 500000.0;
        for (const auto& t : tempos)
        {
            if (t.tick >= tick) break;
            seconds += static_cast<double>(t.tick - prevTick) * micros / (ppq * 1e6);
            prevTick = t.tick;
            micros = t.microsPerQuarter;
        }
        return seconds + static_cast<double>(tick - prevTick) * micros / (ppq * 1e6);
    };

    // Per-event conversion is O(tempo changes); precompute for the common case of few changes.
    seq.events.reserve(raw.size());
    for (const auto& e : raw)
    {
        SequenceEvent s;
        s.time = toSeconds(e.tick);
        s.kind = e.kind; s.channel = e.channel; s.data1 = e.d1; s.data2 = e.d2; s.track = e.track;
        seq.events.push_back(s);
    }
    seq.sortEvents();
    out.ok = true;
    return out;
}

MidiParseResult parseMidiFile(const std::filesystem::path& path)
{
    MidiParseResult r;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        r.error = "File not found: " + path.string();
        return r;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > 64ULL * 1024 * 1024)
    {
        r.error = "The MIDI file is empty or unreasonably large: " + path.string();
        return r;
    }
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!in)
    {
        r.error = "Cannot read MIDI file: " + path.string();
        return r;
    }
    return parseMidiMemory(bytes.data(), bytes.size());
}

}  // namespace realtone
