#include "AudioConvertPanel.h"

#include "../studio/Paths.h"
#include "../studio/Settings.h"

namespace realtone::ui
{

using namespace realtone::studio;

namespace
{
void styleLabel(juce::Label& l, bool header = false)
{
    l.setFont(header ? theme::ui(12.5f, true) : theme::ui(13.0f));
    l.setColour(juce::Label::textColourId, header ? theme::textDim : theme::text);
}
}  // namespace

AudioConvertPanel::AudioConvertPanel(AppContext& context) : ctx_(context)
{
    auto& c = content();
    for (auto* b : {&open_, &analyse_, &preview_, &stop_, &convert_, &reveal_, &mute_, &down12_, &down1_, &up1_, &up12_})
        c.addAndMakeVisible(b);
    for (auto* l : {&fileLabel_, &instrument_, &hint_, &sensitivityL_, &bendRangeL_, &transposeL_})
    {
        styleLabel(*l);
        c.addAndMakeVisible(l);
    }
    for (auto* l : {&sectionSource_, &sectionNotes_, &sectionOutput_})
    {
        styleLabel(*l, true);
        c.addAndMakeVisible(l);
    }
    fileLabel_.setColour(juce::Label::textColourId, theme::textDim);
    hint_.setColour(juce::Label::textColourId, theme::textFaint);
    hint_.setFont(theme::ui(12.5f));
    hint_.setText("Works best on one clean, clearly pitched voice or instrument at a time: a vocal line, a flute, a bass line. "
                  "It does not separate chords, drums or full mixes.", juce::dontSendNotification);

    banner_.setMultiLine(true, true);
    banner_.setReadOnly(true);
    banner_.setCaretVisible(false);
    banner_.setScrollbarsShown(false);
    banner_.setFont(theme::ui(13.0f));
    c.addAndMakeVisible(banner_);

    sensitivity_.addItem("Strict (fewer, cleaner notes)", 1);
    sensitivity_.addItem("Normal", 2);
    sensitivity_.addItem("Lenient (more notes, more mistakes)", 3);
    sensitivity_.setSelectedId(2, juce::dontSendNotification);
    sensitivity_.setTooltip("How clearly pitched a sound must be to count as a note. Change it, then choose Find notes again.");
    bendRange_.addItem("+/- 2 semitones", 1);
    bendRange_.addItem("+/- 7 semitones", 2);
    bendRange_.addItem("+/- 12 semitones", 3);
    bendRange_.setSelectedId(1, juce::dontSendNotification);
    bendRange_.setTooltip("Range used for the pitch-bend events that carry vibrato and slides. 2 is right for most voices.");
    for (auto* x : {&sensitivity_, &bendRange_})
        c.addAndMakeVisible(x);

    transpose_.setSliderStyle(juce::Slider::LinearHorizontal);
    transpose_.setRange(-24, 24, 1);
    transpose_.setValue(0, juce::dontSendNotification);
    transpose_.setDoubleClickReturnValue(true, 0);
    transpose_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    c.addAndMakeVisible(transpose_);
    preserveBends_.setToggleState(true, juce::dontSendNotification);
    preserveBends_.setTooltip("Carries the pitch movement of the original (vibrato, slides, tuning drift) into the instrument. Skipped for instruments that cannot bend, such as pianos.");
    c.addAndMakeVisible(preserveBends_);

    c.addAndMakeVisible(waveform_);
    c.addAndMakeVisible(roll_);
    c.addAndMakeVisible(export_);
    c.addAndMakeVisible(jobBar_);

    roll_.onSelectionChanged = [this](int) { updateInfo(); };
    roll_.onChanged = [this] { updateInfo(); };
    open_.onClick = [this] { chooseFile(); };
    analyse_.onClick = [this] { startAnalysis(); };
    preview_.onClick = [this] { startPreview(); };
    stop_.onClick = [this] { ctx_.preview.stop(); };
    convert_.onClick = [this] { chooseDestination(); };
    reveal_.onClick = [this] { if (lastExport_.existsAsFile()) lastExport_.revealToUser(); };
    reveal_.setVisible(false);
    mute_.onClick = [this] { editSelected(0, true); };
    down12_.onClick = [this] { editSelected(-12, false); };
    down1_.onClick = [this] { editSelected(-1, false); };
    up1_.onClick = [this] { editSelected(1, false); };
    up12_.onClick = [this] { editSelected(12, false); };
    mute_.setTooltip("Switch the selected note off (or back on). Double-clicking a note does the same.");
    for (auto* b : {&analyse_, &preview_, &stop_, &convert_, &mute_, &down12_, &down1_, &up1_, &up12_})
        b->setEnabled(false);

    layout = [this](int w) { return doLayout(w); };
    updateBanner();
    startTimerHz(10);
}

AudioConvertPanel::~AudioConvertPanel() { stopTimer(); }

void AudioConvertPanel::timerCallback()
{
    const bool busy = task_.isRunning();
    const bool haveNotes = analysed_ && !notes_.empty();
    const bool instrumentReady = ctx_.currentInstrument() != nullptr && ctx_.currentInstrument()->state != realtone::InstallState::NotInstalled;
    open_.setEnabled(!busy);
    analyse_.setEnabled(source_ != nullptr && !busy);
    preview_.setEnabled(haveNotes && instrumentReady && !busy);
    convert_.setEnabled(haveNotes && instrumentReady && !busy);
    stop_.setEnabled(ctx_.preview.isPlaying());
    const bool sel = roll_.selected() >= 0 && !busy;
    for (auto* b : {&mute_, &down12_, &down1_, &up1_, &up12_})
        b->setEnabled(sel);

    const double pos = ctx_.preview.isPlaying() ? ctx_.preview.positionSeconds() : -1.0;
    roll_.setPlayhead(pos);
    waveform_.setPlayhead(pos);

    const auto* e = ctx_.currentInstrument();
    juce::String text = e == nullptr ? "No instrument chosen. Pick one on the Play page."
                        : e->state == realtone::InstallState::NotInstalled ? "Instrument: " + juce::String(e->name) + " - samples are not installed. Install them on the Library page."
                                                                           : "Instrument: " + juce::String(e->name);
    if (instrument_.getText() != text)
    {
        instrument_.setText(text, juce::dontSendNotification);
        instrument_.setColour(juce::Label::textColourId, instrumentReady ? theme::textDim : theme::warn);
    }
}

void AudioConvertPanel::setResult(const juce::String& text, juce::Colour colour)
{
    resultText_ = text;
    jobBar_.setMessage(text, colour);
}

void AudioConvertPanel::chooseFile()
{
    const auto start = juce::File(Settings::get().getString(keys::lastAudioFolder));
    chooser_ = std::make_unique<juce::FileChooser>("Open an audio file", start.isDirectory() ? start : juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.flac");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<AudioConvertPanel>(this)](const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->loadFile(fc.getResult());
                          });
}

void AudioConvertPanel::loadFile(const juce::File& file)
{
    if (task_.isRunning())
        return;
    ctx_.preview.stop();
    Settings::get().setString(keys::lastAudioFolder, file.getParentDirectory().getFullPathName());
    auto outcome = std::make_shared<ImportOutcome>();
    fileLabel_.setText(file.getFileName(), juce::dontSendNotification);
    task_.start([file, outcome](BackgroundTask& t) { *outcome = importAudio(file, t); },
                [this, outcome, file, safe = juce::Component::SafePointer<AudioConvertPanel>(this)](bool cancelled) {
                    if (safe == nullptr || cancelled)
                        return;
                    if (!outcome->ok)
                    {
                        source_.reset();
                        notes_.clear();
                        analysed_ = false;
                        waveform_.setClip(nullptr);
                        roll_.setNotes(nullptr, 1.0);
                        setResult("Cannot use this file: " + outcome->error, theme::error);
                        updateBanner();
                        return;
                    }
                    file_ = file;
                    source_ = outcome->clip;
                    sourceInfo_ = juce::String(source_->durationSeconds(), 1) + " s, " + juce::String(source_->sampleRate / 1000.0, 1) + " kHz, " +
                                  (source_->numChannels() == 1 ? "mono" : "stereo") + ", " + outcome->format.toUpperCase();
                    waveform_.setClip(source_);
                    notes_.clear();
                    analysed_ = false;
                    roll_.setNotes(&notes_, source_->durationSeconds());
                    setResult("Loaded " + sourceInfo_ + ". Finding notes...", theme::textDim);
                    updateBanner();
                    resized();
                    startAnalysis();
                });
}

void AudioConvertPanel::startAnalysis()
{
    if (source_ == nullptr || task_.isRunning())
        return;
    realtone::AnalysisOptions opts;
    switch (sensitivity_.getSelectedId())
    {
        case 1: opts.periodicityThreshold = 0.12; opts.minNoteSeconds = 0.09; break;
        case 3: opts.periodicityThreshold = 0.30; opts.minNoteSeconds = 0.04; opts.silenceDb = -58.0; break;
        default: break;
    }
    auto clip = source_;
    auto outcome = std::make_shared<AnalysisOutcome>();
    analysed_ = false;
    task_.start([clip, opts, outcome](BackgroundTask& t) { *outcome = analyseAudio(*clip, opts, t); },
                [this, outcome, safe = juce::Component::SafePointer<AudioConvertPanel>(this)](bool cancelled) {
                    if (safe == nullptr)
                        return;
                    if (cancelled)
                    {
                        setResult("Analysis cancelled.", theme::textDim);
                        return;
                    }
                    report_ = outcome->report;
                    analysed_ = true;
                    if (!outcome->ok)
                    {
                        notes_.clear();
                        roll_.setNotes(&notes_, source_ ? source_->durationSeconds() : 1.0);
                        setResult("No melody found: " + outcome->error, theme::error);
                        updateBanner();
                        return;
                    }
                    notes_ = report_.notes;
                    roll_.setNotes(&notes_, source_->durationSeconds());
                    setResult("Found " + juce::String(static_cast<int>(notes_.size())) + " notes.", theme::ok);
                    updateBanner();
                    updateInfo();
                    resized();
                });
}

void AudioConvertPanel::updateBanner()
{
    juce::String t;
    juce::Colour c = theme::textDim;
    if (source_ == nullptr)
        t = "Open a recording to begin. Your original file is only ever read, never changed.";
    else if (!analysed_)
        t = "Analysing " + sourceInfo_ + "...";
    else if (report_.ok)
    {
        t = "Mean confidence " + juce::String(juce::roundToInt(static_cast<float>(report_.meanConfidence) * 100.0f)) + "%. ";
        if (report_.warnings.empty())
            t << "This looks like a clean single line. Check the notes below, then preview.";
        else
        {
            c = report_.likelyPolyphonic || report_.meanConfidence < 0.75 ? theme::error : theme::warn;
            for (const auto& w : report_.warnings)
                t << juce::String(w) << " ";
        }
    }
    else
    {
        c = theme::error;
        t = "Nothing usable was found. " + juce::String(report_.error) + " Try a recording with a single clear voice, or use a MIDI file instead.";
    }
    banner_.setText(t, false);
    banner_.setColour(juce::TextEditor::textColourId, c);
    banner_.setColour(juce::TextEditor::outlineColourId, c.withAlpha(0.6f));
    banner_.applyColourToAllText(c);
}

void AudioConvertPanel::updateInfo() { roll_.repaint(); }

void AudioConvertPanel::editSelected(int semitones, bool toggleMute)
{
    const int i = roll_.selected();
    if (i < 0 || i >= static_cast<int>(notes_.size()))
        return;
    auto& n = notes_[static_cast<std::size_t>(i)];
    if (toggleMute)
        n.enabled = !n.enabled;
    else
        n.midiNote = juce::jlimit(0, 127, n.midiNote + semitones);
    roll_.repaint();
}

realtone::SequenceFromNotesOptions AudioConvertPanel::sequenceOptions(const realtone::InstrumentProfile& profile, bool& bendsUsed) const
{
    realtone::SequenceFromNotesOptions o;
    const double ranges[] = {2.0, 7.0, 12.0};
    o.bendRangeSemitones = ranges[juce::jlimit(0, 2, bendRange_.getSelectedId() - 1)];
    o.preservePitchContour = preserveBends_.getToggleState() && profile.pitchBendByDefault;
    bendsUsed = o.preservePitchContour;
    return o;
}

realtone::RenderSettings AudioConvertPanel::makeSettings(double sampleRate, const realtone::SequenceFromNotesOptions& so, bool bends) const
{
    auto s = ctx_.baseRenderSettings();
    export_.applyTo(s);
    s.sampleRate = sampleRate;
    s.transposeSemitones = juce::roundToInt(transpose_.getValue());
    s.engine.pitchBendEnabled = bends;
    s.engine.pitchBendRange = static_cast<float>(so.bendRangeSemitones);
    s.includeControllers = false;
    return s;
}

void AudioConvertPanel::startPreview()
{
    const auto* entry = ctx_.currentInstrument();
    if (!analysed_ || notes_.empty() || entry == nullptr || task_.isRunning())
        return;
    ctx_.preview.stop();
    const auto profile = realtone::profileFor(realtone::familyFromString(entry->family));
    bool bends = false;
    const auto so = sequenceOptions(profile, bends);
    const auto seq = realtone::notesToSequence(notes_, so);
    auto settings = makeSettings(ctx_.deviceSampleRate(), so, bends);
    settings.maxTailSeconds = 3.0;
    const auto entryCopy = *entry;
    auto outcome = std::make_shared<RenderOutcome>();
    task_.start([this, entryCopy, seq, settings, outcome](BackgroundTask& t) { *outcome = renderWithInstrument(ctx_.processor.instruments(), entryCopy, seq, settings, t); },
                [this, outcome, safe = juce::Component::SafePointer<AudioConvertPanel>(this)](bool cancelled) {
                    if (safe == nullptr)
                        return;
                    if (cancelled || outcome->cancelled)
                    {
                        setResult("Preview cancelled.", theme::textDim);
                        return;
                    }
                    if (!outcome->ok)
                    {
                        setResult("Preview failed: " + outcome->error, theme::error);
                        return;
                    }
                    ctx_.preview.play(outcome->clip);
                    setResult("Playing the converted result...", theme::textDim);
                });
}

void AudioConvertPanel::chooseDestination()
{
    const auto* entry = ctx_.currentInstrument();
    if (!analysed_ || notes_.empty() || entry == nullptr || task_.isRunning())
        return;
    const auto folder = juce::File(Settings::get().getString(keys::lastExportFolder, file_.getParentDirectory().getFullPathName()));
    const auto suggestion = suggestOutputFile(folder.isDirectory() ? folder : file_.getParentDirectory(), file_.getFileNameWithoutExtension(), juce::String(entry->name));
    chooser_ = std::make_unique<juce::FileChooser>("Save the converted audio as WAV", suggestion, "*.wav");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<AudioConvertPanel>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              safe->convertTo(fc.getResult().withFileExtension("wav"), true);
                          });
}

void AudioConvertPanel::convertTo(const juce::File& destination, bool allowOverwrite)
{
    const auto* entry = ctx_.currentInstrument();
    if (!analysed_ || notes_.empty() || entry == nullptr || task_.isRunning())
        return;
    ctx_.preview.stop();
    Settings::get().setString(keys::lastExportFolder, destination.getParentDirectory().getFullPathName());
    const auto profile = realtone::profileFor(realtone::familyFromString(entry->family));
    bool bends = false;
    const auto so = sequenceOptions(profile, bends);
    const auto seq = realtone::notesToSequence(notes_, so);
    const auto settings = makeSettings(export_.sampleRate(), so, bends);
    const auto format = export_.format();
    const auto entryCopy = *entry;
    const auto source = file_;
    const bool bendSkipped = preserveBends_.getToggleState() && !bends;
    const double confidence = report_.meanConfidence;
    auto result = std::make_shared<juce::String>();
    auto colour = std::make_shared<juce::Colour>(theme::textDim);
    reveal_.setVisible(false);

    task_.start(
        [this, entryCopy, seq, settings, format, destination, allowOverwrite, source, bendSkipped, confidence, result, colour](BackgroundTask& t) {
            auto outcome = renderWithInstrument(ctx_.processor.instruments(), entryCopy, seq, settings, t);
            if (!outcome.ok)
            {
                *result = outcome.cancelled ? "Conversion was cancelled. Nothing was written." : "Conversion failed: " + outcome.error;
                *colour = outcome.cancelled ? theme::textDim : theme::error;
                return;
            }
            t.report(0.98f, "Writing " + destination.getFileName());
            auto exported = exportWav(*outcome.clip, destination, format, allowOverwrite, source);
            if (!exported.ok)
            {
                *result = "Could not save: " + exported.error;
                *colour = theme::error;
                return;
            }
            juce::String msg = "Saved " + destination.getFullPathName() + "  (" + juce::String(outcome.clip->durationSeconds(), 1) + " s, " +
                               juce::String(outcome.notes) + " notes).";
            if (bendSkipped)
                msg << "  Pitch slides were not applied because " << juce::String(entryCopy.name) << " cannot bend notes.";
            if (confidence < 0.75)
                msg << "  Detection confidence was low, so expect mistakes.";
            for (const auto& w : outcome.warnings)
                msg << "  " << juce::String(w);
            *result = msg;
            *colour = confidence < 0.75 ? theme::warn : theme::ok;
        },
        [this, result, colour, destination, safe = juce::Component::SafePointer<AudioConvertPanel>(this)](bool) {
            if (safe == nullptr)
                return;
            setResult(*result, *colour);
            if (destination.existsAsFile() && *colour != theme::error)
            {
                lastExport_ = destination;
                reveal_.setVisible(true);
                resized();
            }
        });
}

int AudioConvertPanel::doLayout(int width)
{
    constexpr int gap = 12;
    const int pad = 14;
    const int inner = width - pad * 2;
    int y = pad;

    open_.setBounds(pad, y, 170, theme::controlHeight);
    analyse_.setBounds(pad + 178, y, 150, theme::controlHeight);
    fileLabel_.setBounds(pad + 336, y, juce::jmax(60, inner - 336), theme::controlHeight);
    y += theme::controlHeight + 6;
    instrument_.setBounds(pad, y, inner, 20);
    y += 24;
    hint_.setBounds(pad, y, inner, hint_.getFont().getHeight() * (inner < 700 ? 3.2f : 2.2f));
    y += static_cast<int>(hint_.getFont().getHeight() * (inner < 700 ? 3.2f : 2.2f)) + 6;

    banner_.setBounds(pad, y, inner, inner < 700 ? 84 : 58);
    y += (inner < 700 ? 84 : 58) + gap;

    sectionSource_.setBounds(pad, y, inner, 20);
    y += 24;
    waveform_.setBounds(pad, y, inner, 90);
    y += 90 + gap;

    sectionNotes_.setBounds(pad, y, inner, 20);
    y += 24;
    roll_.setBounds(pad, y, inner, 190);
    y += 190 + 8;
    const int eb = juce::jmax(60, juce::jmin(110, (inner - 4 * 6) / 5));
    int x = pad;
    for (auto* b : {&mute_, &down12_, &down1_, &up1_, &up12_})
    {
        b->setBounds(x, y, b == &mute_ ? eb + 40 : eb - 20, theme::controlHeight);
        x += (b == &mute_ ? eb + 40 : eb - 20) + 6;
    }
    y += theme::controlHeight + gap + 6;

    const bool two = width >= 860;
    const int colGap = 20;
    const int leftW = two ? (inner - colGap) / 2 : inner;
    const int rightW = two ? inner - colGap - leftW : inner;
    int ly = y;
    sectionOutput_.setBounds(pad, ly, leftW, 20);
    ly += 26;
    auto row = [&](juce::Label& l, juce::Component& c, int x0, int w0, int& yy) {
        const int lw = juce::jmin(150, w0 / 2);
        l.setBounds(x0, yy, lw, 24);
        c.setBounds(x0 + lw, yy, w0 - lw, theme::controlHeight - 2);
        yy += theme::controlHeight + 4;
    };
    row(sensitivityL_, sensitivity_, pad, leftW, ly);
    row(bendRangeL_, bendRange_, pad, leftW, ly);
    row(transposeL_, transpose_, pad, leftW, ly);
    preserveBends_.setBounds(pad, ly, leftW, 24);
    ly += 30;

    int ry = two ? y + 26 : ly + gap;
    const int rx = two ? pad + leftW + colGap : pad;
    export_.setBounds(rx, ry, rightW, ExportOptionsBox::kPreferredHeight);
    ry += ExportOptionsBox::kPreferredHeight + gap;
    y = juce::jmax(ly, ry);

    const int bw = juce::jmax(90, juce::jmin(150, (inner - 3 * 8) / 4));
    preview_.setBounds(pad, y, bw, theme::controlHeight);
    stop_.setBounds(pad + bw + 8, y, bw, theme::controlHeight);
    convert_.setBounds(pad + 2 * (bw + 8), y, bw + 90, theme::controlHeight);
    reveal_.setBounds(pad + 2 * (bw + 8) + bw + 98, y, bw, theme::controlHeight);
    y += theme::controlHeight + gap;
    jobBar_.setBounds(pad, y, inner, JobBar::kHeight);
    y += JobBar::kHeight + pad;
    return y;
}

juce::ValueTree AudioConvertPanel::state() const
{
    juce::ValueTree v("AudioConvert", {{"file", source_ != nullptr ? file_.getFullPathName() : juce::String()},
                                       {"sensitivity", sensitivity_.getSelectedId()},
                                       {"bendRange", bendRange_.getSelectedId()},
                                       {"transpose", transpose_.getValue()},
                                       {"preserveBends", preserveBends_.getToggleState()}});
    v.appendChild(export_.state(), nullptr);
    return v;
}

void AudioConvertPanel::setState(const juce::ValueTree& v)
{
    if (!v.isValid())
        return;
    sensitivity_.setSelectedId(juce::jlimit(1, 3, static_cast<int>(v.getProperty("sensitivity", 2))), juce::dontSendNotification);
    bendRange_.setSelectedId(juce::jlimit(1, 3, static_cast<int>(v.getProperty("bendRange", 1))), juce::dontSendNotification);
    transpose_.setValue(v.getProperty("transpose", 0), juce::dontSendNotification);
    preserveBends_.setToggleState(v.getProperty("preserveBends", true), juce::dontSendNotification);
    export_.setState(v.getChildWithName("Export"));
    const juce::File f(v.getProperty("file").toString());
    if (f.existsAsFile())
        loadFile(f);
    else if (f != juce::File())
        setResult("The audio file from this project was not found: " + f.getFullPathName(), theme::warn);
}

}  // namespace realtone::ui
