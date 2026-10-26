#include "MidiRenderPanel.h"

#include "../studio/Paths.h"
#include "../studio/Settings.h"

namespace realtone::ui
{

using namespace realtone::studio;

namespace
{
void styleSlider(juce::Slider& s, double lo, double hi, double step, double def, const juce::String& suffix = {})
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setRange(lo, hi, step);
    s.setValue(def, juce::dontSendNotification);
    s.setDoubleClickReturnValue(true, def);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}
void styleLabel(juce::Label& l, bool header = false)
{
    l.setFont(header ? theme::ui(12.5f, true) : theme::ui(13.0f));
    l.setColour(juce::Label::textColourId, header ? theme::textDim : theme::text);
}
juce::String clock(double s)
{
    const int t = juce::roundToInt(s);
    return juce::String(t / 60) + ":" + juce::String(t % 60).paddedLeft('0', 2);
}
}  // namespace

MidiRenderPanel::MidiRenderPanel(AppContext& context) : ctx_(context)
{
    auto& c = content();
    for (auto* b : {&open_, &preview_, &stop_, &render_, &reveal_})
        c.addAndMakeVisible(b);
    for (auto* l : {&fileLabel_, &summary_, &instrument_, &hint_, &transposeL_, &tempoL_, &velocityL_, &humanizeL_, &tailL_})
    {
        styleLabel(*l);
        c.addAndMakeVisible(l);
    }
    for (auto* l : {&sectionTracks_, &sectionEdit_, &sectionExport_})
    {
        styleLabel(*l, true);
        c.addAndMakeVisible(l);
    }
    fileLabel_.setColour(juce::Label::textColourId, theme::textDim);
    summary_.setColour(juce::Label::textColourId, theme::textDim);
    hint_.setColour(juce::Label::textColourId, theme::textFaint);
    hint_.setFont(theme::ui(12.5f));
    hint_.setText("Drop a .mid file here, or use Open. Each track is played by the instrument chosen on the Play page.", juce::dontSendNotification);

    styleSlider(transpose_, -24, 24, 1, 0);
    styleSlider(tempo_, 0.5, 2.0, 0.01, 1.0, "x");
    styleSlider(velocity_, 0.5, 1.5, 0.01, 1.0, "x");
    styleSlider(humanize_, 0, 30, 1, 0, " ms");
    styleSlider(tail_, 1, 20, 1, 10, " s");
    for (auto* s : {&transpose_, &tempo_, &velocity_, &humanize_, &tail_})
        c.addAndMakeVisible(s);
    tempo_.setTooltip("1.00 keeps the tempo from the file. 2.00 plays twice as fast.");
    humanize_.setTooltip("Shifts each note by a small random amount (the same every time you render).");
    tail_.setTooltip("Rendering continues after the last note until the sound has died away, up to this limit.");

    controllers_.setToggleState(true, juce::dontSendNotification);
    bend_.setToggleState(true, juce::dontSendNotification);
    c.addAndMakeVisible(controllers_);
    c.addAndMakeVisible(bend_);
    c.addAndMakeVisible(export_);

    tracks_.setRowHeight(32);
    tracks_.setOutlineThickness(1);
    c.addAndMakeVisible(tracks_);

    warnings_.setMultiLine(true, true);
    warnings_.setReadOnly(true);
    warnings_.setCaretVisible(false);
    warnings_.setFont(theme::ui(12.5f));
    c.addAndMakeVisible(warnings_);
    c.addAndMakeVisible(jobBar_);

    open_.onClick = [this] { chooseFile(); };
    preview_.onClick = [this] { startPreview(); };
    stop_.onClick = [this] { ctx_.preview.stop(); };
    render_.onClick = [this] { chooseDestination(); };
    reveal_.onClick = [this] { if (lastExport_.existsAsFile()) lastExport_.revealToUser(); };
    reveal_.setVisible(false);
    preview_.setTooltip("Render the first part through the chosen instrument and play it.");
    render_.setTooltip("Render the whole file and save it as a WAV. Your MIDI file is never changed.");
    for (auto* b : {&preview_, &stop_, &render_})
        b->setEnabled(false);

    layout = [this](int w) { return doLayout(w); };
    updateSummary();
    updateInstrumentLine();
    startTimerHz(4);
}

MidiRenderPanel::~MidiRenderPanel() { stopTimer(); }

void MidiRenderPanel::timerCallback()
{
    updateInstrumentLine();
    const bool busy = task_.isRunning();
    const bool ready = loaded_ && ctx_.currentInstrument() != nullptr && !busy;
    preview_.setEnabled(ready);
    render_.setEnabled(ready);
    stop_.setEnabled(ctx_.preview.isPlaying());
    open_.setEnabled(!busy);
}

void MidiRenderPanel::updateInstrumentLine()
{
    const auto* e = ctx_.currentInstrument();
    const auto st = ctx_.processor.instruments().status();
    juce::String text;
    juce::Colour colour = theme::textDim;
    if (e == nullptr)
    {
        text = "No instrument chosen. Pick one on the Play page.";
        colour = theme::warn;
    }
    else if (e->state == realtone::InstallState::NotInstalled)
    {
        text = "Instrument: " + juce::String(e->name) + " - samples are not installed. Install them on the Library page.";
        colour = theme::warn;
    }
    else
        text = "Instrument: " + juce::String(e->name) + (st.state == LoadStatus::State::ReadyPartial ? "  (some samples missing)" : "");
    if (instrument_.getText() != text)
    {
        instrument_.setText(text, juce::dontSendNotification);
        instrument_.setColour(juce::Label::textColourId, colour);
    }
}

void MidiRenderPanel::chooseFile()
{
    const auto start = juce::File(Settings::get().getString(keys::lastMidiFolder));
    chooser_ = std::make_unique<juce::FileChooser>("Open a MIDI file", start.isDirectory() ? start : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                   "*.mid;*.midi");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<MidiRenderPanel>(this)](const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->loadFile(fc.getResult());
                          });
}

void MidiRenderPanel::loadFile(const juce::File& file)
{
    if (task_.isRunning())
        return;  // a render is using the current sequence
    auto parsed = realtone::parseMidiFile(std::filesystem::path(file.getFullPathName().toStdString()));
    if (!parsed.ok)
    {
        loaded_ = false;
        rows_.clear();
        tracks_.updateContent();
        fileLabel_.setText(file.getFileName(), juce::dontSendNotification);
        updateSummary();
        warnings_.setText({}, false);
        setResult("Cannot use this file: " + juce::String(parsed.error), theme::error);
        logMessage("MIDI import failed (" + file.getFullPathName() + "): " + juce::String(parsed.error));
        return;
    }
    Settings::get().setString(keys::lastMidiFolder, file.getParentDirectory().getFullPathName());
    file_ = file;
    sequence_ = std::move(parsed.sequence);
    loaded_ = true;
    rows_.clear();
    for (const auto& t : sequence_.tracks)
        if (t.noteCount > 0)
            rows_.push_back({t, !t.looksLikePercussion});
    // Everything percussive is off by default: a pitched instrument cannot play drum maps.
    bool anyOn = std::any_of(rows_.begin(), rows_.end(), [](const Row& r) { return r.enabled; });
    if (!anyOn)
        for (auto& r : rows_)
            r.enabled = true;

    fileLabel_.setText(file.getFileName(), juce::dontSendNotification);
    juce::String w;
    for (const auto& s : sequence_.warnings)
        w << "- " << juce::String(s) << "\n";
    if (std::any_of(rows_.begin(), rows_.end(), [](const Row& r) { return r.info.looksLikePercussion; }))
        w << "- A track on MIDI channel 10 looks like drums. It is switched off because a pitched instrument cannot play a drum map.\n";
    warnings_.setText(w, false);
    tracks_.updateContent();
    updateSummary();
    setResult({}, theme::textDim);
    resized();
}

void MidiRenderPanel::updateSummary()
{
    if (!loaded_)
    {
        summary_.setText("No MIDI file loaded.", juce::dontSendNotification);
        return;
    }
    summary_.setText(juce::String(sequence_.noteCount) + " notes, " + juce::String(static_cast<int>(rows_.size())) + " track(s), " +
                         juce::String(sequence_.initialTempoBpm, 1) + " bpm, " + clock(sequence_.durationSeconds),
                     juce::dontSendNotification);
}

void MidiRenderPanel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    using namespace theme;
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    const auto& r = rows_[static_cast<std::size_t>(row)];
    if (selected)
    {
        g.setColour(accentSoft.withAlpha(0.5f));
        g.fillRect(0, 0, w, h);
    }
    g.setColour(border);
    g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));
    // checkbox
    const auto box = juce::Rectangle<float>(10.0f, (static_cast<float>(h) - 16.0f) * 0.5f, 16.0f, 16.0f);
    g.setColour(r.enabled ? accent : surfaceHover);
    g.fillRoundedRectangle(box, 4.0f);
    if (r.enabled)
    {
        g.setColour(background);
        juce::Path tick;
        tick.startNewSubPath(box.getX() + 3.5f, box.getCentreY());
        tick.lineTo(box.getX() + 7.0f, box.getBottom() - 4.0f);
        tick.lineTo(box.getRight() - 3.0f, box.getY() + 4.0f);
        g.strokePath(tick, juce::PathStrokeType(2.0f));
    }
    g.setColour(r.enabled ? text : textFaint);
    g.setFont(ui(13.5f));
    const auto name = r.info.name.empty() ? "Track " + juce::String(r.info.index + 1) : juce::String(r.info.name);
    g.drawText(name, 36, 0, w - 36 - 190, h, juce::Justification::centredLeft);
    g.setColour(textFaint);
    g.setFont(ui(12.0f));
    juce::String info = juce::String(r.info.noteCount) + " notes";
    if (r.info.firstChannel >= 0)
        info << "  ch " << (r.info.firstChannel + 1);
    if (r.info.looksLikePercussion)
        info << "  drums";
    g.drawText(info, w - 190, 0, 180, h, juce::Justification::centredRight);
}

void MidiRenderPanel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < static_cast<int>(rows_.size()))
    {
        rows_[static_cast<std::size_t>(row)].enabled = !rows_[static_cast<std::size_t>(row)].enabled;
        tracks_.repaint();
    }
}

realtone::RenderSettings MidiRenderPanel::makeSettings(double sampleRate) const
{
    auto s = ctx_.baseRenderSettings();
    export_.applyTo(s);
    s.sampleRate = sampleRate;
    s.transposeSemitones = juce::roundToInt(transpose_.getValue());
    s.tempoScale = tempo_.getValue();
    s.velocityScale = static_cast<float>(velocity_.getValue());
    s.timingHumanizeMs = humanize_.getValue();
    s.maxTailSeconds = tail_.getValue();
    s.includeControllers = controllers_.getToggleState();
    s.includePitchBend = bend_.getToggleState();
    for (const auto& r : rows_)
        if (r.enabled)
            s.enabledTracks.push_back(r.info.index);
    return s;
}

void MidiRenderPanel::setResult(const juce::String& text, juce::Colour colour)
{
    resultText_ = text;
    jobBar_.setMessage(text, colour);
}

void MidiRenderPanel::startPreview()
{
    const auto* entry = ctx_.currentInstrument();
    if (!loaded_ || entry == nullptr || task_.isRunning())
        return;
    ctx_.preview.stop();
    auto settings = makeSettings(ctx_.deviceSampleRate());
    settings.maxTailSeconds = 3.0;

    // Preview the first 40 seconds so it starts quickly.
    realtone::Sequence part = sequence_;
    part.events.erase(std::remove_if(part.events.begin(), part.events.end(), [](const realtone::SequenceEvent& e) { return e.time > 40.0 * 1.0; }),
                      part.events.end());
    // Notes cut off by the limit get closed by the renderer.
    const auto entryCopy = *entry;
    auto outcome = std::make_shared<RenderOutcome>();
    task_.start(
        [this, entryCopy, part, settings, outcome](BackgroundTask& t) {
            *outcome = renderWithInstrument(ctx_.processor.instruments(), entryCopy, part, settings, t);
        },
        [this, outcome, safe = juce::Component::SafePointer<MidiRenderPanel>(this)](bool cancelled) {
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
            setResult("Playing preview...", theme::textDim);
        });
}

void MidiRenderPanel::chooseDestination()
{
    const auto* entry = ctx_.currentInstrument();
    if (!loaded_ || entry == nullptr || task_.isRunning())
        return;
    const auto folder = juce::File(Settings::get().getString(keys::lastExportFolder, file_.getParentDirectory().getFullPathName()));
    const auto suggestion = suggestOutputFile(folder.isDirectory() ? folder : file_.getParentDirectory(), file_.getFileNameWithoutExtension(), juce::String(entry->name));
    chooser_ = std::make_unique<juce::FileChooser>("Save the rendered audio as WAV", suggestion, "*.wav");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<MidiRenderPanel>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              // The chooser has already asked the user about replacing an existing file.
                              safe->renderTo(fc.getResult().withFileExtension("wav"), true);
                          });
}

void MidiRenderPanel::renderTo(const juce::File& destination, bool allowOverwrite)
{
    const auto* entry = ctx_.currentInstrument();
    if (!loaded_ || entry == nullptr || task_.isRunning())
        return;
    ctx_.preview.stop();
    Settings::get().setString(keys::lastExportFolder, destination.getParentDirectory().getFullPathName());
    const auto settings = makeSettings(export_.sampleRate());
    const auto format = export_.format();
    const auto entryCopy = *entry;
    const auto source = file_;
    const auto seq = sequence_;  // the worker gets its own copy
    auto result = std::make_shared<juce::String>();
    auto colour = std::make_shared<juce::Colour>(theme::textDim);
    reveal_.setVisible(false);

    task_.start(
        [this, entryCopy, settings, format, destination, allowOverwrite, source, seq, result, colour](BackgroundTask& t) {
            auto outcome = renderWithInstrument(ctx_.processor.instruments(), entryCopy, seq, settings, t);
            if (!outcome.ok)
            {
                *result = outcome.cancelled ? "Rendering was cancelled. Nothing was written." : "Render failed: " + outcome.error;
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
                               juce::String(outcome.clip->sampleRate / 1000.0, 1) + " kHz, " + juce::String(outcome.notes) + " notes";
            if (outcome.skipped > 0)
                msg << ", " << outcome.skipped << " skipped";
            msg << ")";
            if (outcome.gainDb < -0.05)
                msg << "  Level lowered by " + juce::String(-outcome.gainDb, 1) + " dB to avoid clipping.";
            for (const auto& w : outcome.warnings)
                msg << "  " << juce::String(w);
            *result = msg;
            *colour = theme::ok;
        },
        [this, result, colour, destination, safe = juce::Component::SafePointer<MidiRenderPanel>(this)](bool) {
            if (safe == nullptr)
                return;
            setResult(*result, *colour);
            if (*colour == theme::ok)
            {
                lastExport_ = destination;
                reveal_.setVisible(true);
                resized();
            }
        });
}

int MidiRenderPanel::doLayout(int width)
{
    constexpr int gap = 12;
    const int pad = 14;
    const bool twoColumns = width >= 860;
    const int inner = width - pad * 2;
    int y = pad;

    // Top: open + file name
    open_.setBounds(pad, y, 150, theme::controlHeight);
    fileLabel_.setBounds(pad + 160, y, inner - 160, theme::controlHeight);
    y += theme::controlHeight + 4;
    summary_.setBounds(pad, y, inner, 20);
    y += 22;
    instrument_.setBounds(pad, y, inner, 20);
    y += 26;
    hint_.setBounds(pad, y, inner, 20);
    y += 28;

    const int colGap = 20;
    const int leftW = twoColumns ? (inner - colGap) / 2 : inner;
    const int rightW = twoColumns ? inner - colGap - leftW : inner;

    // Left column: tracks and notes about the file
    int ly = y;
    sectionTracks_.setBounds(pad, ly, leftW, 20);
    ly += 24;
    const int listH = juce::jlimit(110, 260, static_cast<int>(rows_.size()) * 32 + 4);
    tracks_.setBounds(pad, ly, leftW, listH);
    ly += listH + gap;
    warnings_.setBounds(pad, ly, leftW, warnings_.getText().isEmpty() ? 0 : 80);
    ly += (warnings_.getText().isEmpty() ? 0 : 80 + gap);

    // Right column (or below): edits and export
    int ry = twoColumns ? y : ly;
    const int rx = twoColumns ? pad + leftW + colGap : pad;
    sectionEdit_.setBounds(rx, ry, rightW, 20);
    ry += 26;
    auto sliderRow = [&](juce::Label& l, juce::Slider& s) {
        const int labelW = juce::jmin(190, rightW / 2);
        l.setBounds(rx, ry, labelW, 24);
        s.setBounds(rx + labelW, ry, rightW - labelW, 24);
        ry += 30;
    };
    sliderRow(transposeL_, transpose_);
    sliderRow(tempoL_, tempo_);
    sliderRow(velocityL_, velocity_);
    sliderRow(humanizeL_, humanize_);
    sliderRow(tailL_, tail_);
    controllers_.setBounds(rx, ry, rightW, 24);
    ry += 28;
    bend_.setBounds(rx, ry, rightW, 24);
    ry += 36;

    sectionExport_.setBounds(rx, ry, rightW, 20);
    ry += 26;
    export_.setBounds(rx, ry, rightW, ExportOptionsBox::kPreferredHeight);
    ry += ExportOptionsBox::kPreferredHeight + gap;

    y = juce::jmax(ly, ry);

    // Actions
    const int bw = juce::jmax(90, juce::jmin(160, (inner - 3 * 8) / 4));
    preview_.setBounds(pad, y, bw, theme::controlHeight);
    stop_.setBounds(pad + bw + 8, y, bw, theme::controlHeight);
    render_.setBounds(pad + 2 * (bw + 8), y, bw + 40, theme::controlHeight);
    reveal_.setBounds(pad + 2 * (bw + 8) + bw + 48, y, bw, theme::controlHeight);
    y += theme::controlHeight + gap;
    jobBar_.setBounds(pad, y, inner, JobBar::kHeight);
    y += JobBar::kHeight + pad;
    return y;
}

juce::ValueTree MidiRenderPanel::state() const
{
    juce::ValueTree v("MidiRender", {{"file", loaded_ ? file_.getFullPathName() : juce::String()},
                                     {"transpose", transpose_.getValue()},
                                     {"tempo", tempo_.getValue()},
                                     {"velocity", velocity_.getValue()},
                                     {"humanize", humanize_.getValue()},
                                     {"tail", tail_.getValue()},
                                     {"controllers", controllers_.getToggleState()},
                                     {"bend", bend_.getToggleState()}});
    for (const auto& r : rows_)
        v.appendChild(juce::ValueTree("Track", {{"index", r.info.index}, {"enabled", r.enabled}}), nullptr);
    v.appendChild(export_.state(), nullptr);
    return v;
}

void MidiRenderPanel::setState(const juce::ValueTree& v)
{
    if (!v.isValid())
        return;
    transpose_.setValue(v.getProperty("transpose", 0), juce::dontSendNotification);
    tempo_.setValue(v.getProperty("tempo", 1.0), juce::dontSendNotification);
    velocity_.setValue(v.getProperty("velocity", 1.0), juce::dontSendNotification);
    humanize_.setValue(v.getProperty("humanize", 0), juce::dontSendNotification);
    tail_.setValue(v.getProperty("tail", 10), juce::dontSendNotification);
    controllers_.setToggleState(v.getProperty("controllers", true), juce::dontSendNotification);
    bend_.setToggleState(v.getProperty("bend", true), juce::dontSendNotification);
    export_.setState(v.getChildWithName("Export"));
    const juce::File f(v.getProperty("file").toString());
    if (f.existsAsFile())
    {
        loadFile(f);
        for (auto track : v)
            if (track.hasType("Track"))
                for (auto& r : rows_)
                    if (r.info.index == static_cast<int>(track.getProperty("index")))
                        r.enabled = track.getProperty("enabled");
        tracks_.repaint();
    }
    else if (f != juce::File())
        setResult("The MIDI file from this project was not found: " + f.getFullPathName(), theme::warn);
}

}  // namespace realtone::ui
