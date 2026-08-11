#include "CompanionEditor.h"

#include <BinaryDataWebUI.h>
#include <FileExport.h>
#include <FileImport.h>

namespace
{
    juce::var objectWith (std::initializer_list<std::pair<const char*, juce::var>> properties)
    {
        auto* object = new juce::DynamicObject();
        for (const auto& p : properties)
            object->setProperty (juce::Identifier (p.first), p.second);
        return juce::var (object);
    }

    int intFrom (const juce::var& v, const char* key, int fallback = 0)
    {
        return static_cast<int> (v.getProperty (key, fallback));
    }

    std::optional<std::uint32_t> seedFrom (const juce::var& v)
    {
        const auto property = v.getProperty ("seed", juce::var());

        // The page may send a number or the canonical hex string; accept both
        // rather than making the JS remember which.
        if (property.isString())
            return rnd::parseSeed (property.toString().toStdString());

        if (property.isVoid())
            return std::nullopt;

        return static_cast<std::uint32_t> (static_cast<juce::int64> (property));
    }
}

//==============================================================================
CompanionEditor::CompanionEditor (CompanionProcessor& p)
    : AudioProcessorEditor (&p),
      model (p.model()),
      web ({
               { "/index.html", { BinaryData::index_html, BinaryData::index_htmlSize, "text/html; charset=utf-8" } },
               { "/bundle.js",  { BinaryData::bundle_js,  BinaryData::bundle_jsSize,  "application/javascript" } },
           },
           makeEvents())
{
    addAndMakeVisible (web);
    web.start();

    model.onStatusChanged = [this] { pushStatus(); };
    model.onLog           = [this] (const juce::String& text) { log (text); };
    model.library().onChanged = [this] { pushLibrary(); };
    model.link().onConnectionChanged = [this] { pushPorts(); };

    setResizable (true, true);
    setResizeLimits (420, 460, 4000, 3000);
    setSize (1040, 780);

    // Ports come and go while the plugin runs; a slow poll keeps the page
    // honest without a platform-specific hotplug callback.
    startTimer (2000);
}

CompanionEditor::~CompanionEditor()
{
    stopTimer();

    // The model outlives this editor -- leave it holding no dangling callbacks.
    model.onStatusChanged = nullptr;
    model.onLog = nullptr;
    model.library().onChanged = nullptr;
    model.link().onConnectionChanged = nullptr;
}

void CompanionEditor::resized()
{
    web.setBounds (getLocalBounds());
}

void CompanionEditor::timerCallback()
{
    // Only when something actually changed. Any cluster update re-renders it,
    // which closes an open popover -- so pushing unconditionally every two
    // seconds meant the MIDI panel shut itself before you could use it.
    if (! pageReady)
        return;

    // Plugging the RND in after the plugin opened should just work.
    if (model.autoConnectIfIdle())
        log ("Connected to " + model.link().outputName());

    pushPorts (OnlyIfChanged::yes);
}

//==============================================================================
SeedEntry::Rating CompanionEditor::ratingFromVar (const juce::var& v)
{
    const auto rating = v.getProperty ("rating", "unrated").toString();

    if (rating == "keep") return SeedEntry::Rating::keep;
    if (rating == "pass") return SeedEntry::Rating::pass;
    return SeedEntry::Rating::unrated;
}

enkerli::BridgedWebView::EventMap CompanionEditor::makeEvents()
{
    return {
        { "uiReady", [this] (const juce::var&)
            {
                pageReady = true;

                // Open the RND before the first paint, so the device pickers
                // show it selected rather than blank.
                model.autoConnectIfIdle();

                pushTransport();
                pushPorts();
                pushStatus();
                pushLibrary();
                log ("RND Companion ready. Connect the RND over USB, then press Find RND.");
            } },

        // ── Device ──────────────────────────────────────────────────────────
        { "sendSeed", [this] (const juce::var& v)
            {
                if (const auto seed = seedFrom (v))
                    model.sendSeed (*seed);
            } },
        { "readDevice", [this] (const juce::var&) { model.requestStatusDump(); } },
        { "capture", [this] (const juce::var&)
            {
                if (const auto seed = model.status().seed)
                {
                    model.library().captureSeed (*seed, model.status());
                    log ("Captured " + juce::String (rnd::formatSeed (*seed)));
                }
                else
                {
                    log ("Nothing to capture yet.");
                }
            } },

        // ── Live ────────────────────────────────────────────────────────────
        { "sendScale",  [this] (const juce::var& v) { model.sendScale (intFrom (v, "index")); } },
        { "sendRoot",   [this] (const juce::var& v) { model.sendTonic (intFrom (v, "pitchClass")); } },
        { "sendVolume", [this] (const juce::var& v) { model.sendVolume (intFrom (v, "value", 100), false); } },
        { "sendReverb", [this] (const juce::var& v) { model.sendReverb (intFrom (v, "value", 40), false); } },

        // ── Transport and ports ─────────────────────────────────────────────
        { "setTransport", [this] (const juce::var& v)
            {
                const auto name = v.getProperty ("transport", "direct").toString();
                model.setTransport (name == "host" ? CompanionModel::Transport::host
                                    : name == "both" ? CompanionModel::Transport::both
                                                     : CompanionModel::Transport::direct);
                pushTransport();
            } },
        { "openInput",  [this] (const juce::var& v)
            { model.link().openInput (v.getProperty ("id", "").toString()); pushPorts(); } },
        { "openOutput", [this] (const juce::var& v)
            { model.link().openOutput (v.getProperty ("id", "").toString()); pushPorts(); } },
        { "findDevice", [this] (const juce::var&) { model.link().connectToRnd(); pushPorts(); } },
        { "rescan",     [this] (const juce::var&) { pushPorts(); } },

        // ── Library ─────────────────────────────────────────────────────────
        { "rate", [this] (const juce::var& v)
            {
                if (const auto seed = seedFrom (v))
                    model.library().setRating (*seed, ratingFromVar (v));
            } },
        { "setNote", [this] (const juce::var& v)
            {
                if (const auto seed = seedFrom (v))
                    model.library().setNote (*seed, v.getProperty ("note", "").toString());
            } },
        { "remove", [this] (const juce::var& v)
            {
                const auto seed = seedFrom (v);
                if (! seed)
                    return;

                // The undo lives in the page's toast; C++ just holds the entry
                // long enough for the page to ask for it back.
                if (const auto removed = model.library().remove (*seed))
                    log ("Removed " + removed->displayName() + " -- undo available for a few seconds");
            } },
        { "restore", [this] (const juce::var& v)
            {
                if (const auto seed = seedFrom (v))
                {
                    SeedEntry entry;
                    entry.seed = *seed;
                    entry.note = v.getProperty ("note", "").toString();
                    entry.rating = ratingFromVar (v);
                    model.library().reinsert (entry);
                    log ("Restored " + entry.displayName());
                }
            } },

        // ── Files. WKWebView can neither download nor upload (TESTING.md), so
        //    both go through the suite's native helpers.
        { "exportLibrary", [this] (const juce::var&)
            {
                const auto json = model.library().toJsonString();
                juce::MemoryBlock bytes (json.toRawUTF8(), json.getNumBytesAsUTF8());
                enkerli::exportBytes (*this, SeedLibrary::timestampedExportName(), bytes);
                log ("Exporting " + SeedLibrary::timestampedExportName());
            } },
        { "importLibrary", [this] (const juce::var&)
            {
                // The picker outlives nothing in particular: close the plugin
                // window while it is open and the callback lands on a dead
                // editor. SafePointer is the documented guard (TESTING.md).
                juce::Component::SafePointer<CompanionEditor> safe (this);

                enkerli::importFile (*this, "*.json",
                                     [safe] (const juce::String& name, const juce::MemoryBlock& bytes)
                                     {
                                         if (safe == nullptr)
                                             return;

                                         const juce::String text (juce::CharPointer_UTF8 (
                                             static_cast<const char*> (bytes.getData())), bytes.getSize());

                                         safe->log (safe->model.library().importJsonString (text)
                                                        ? "Imported " + name
                                                        : "Could not read " + name);
                                     });
            } },

        { "log", [this] (const juce::var& v) { log (v.getProperty ("text", "").toString()); } },
    };
}

//==============================================================================
void CompanionEditor::pushStatus()
{
    if (! pageReady)
        return;

    const auto& s = model.status();

    juce::Array<juce::var> engines;
    for (const auto& engine : s.engines)
        engines.add (juce::String (engine.name));

    web.emit ("status", objectWith ({
        { "seed",       s.seed ? juce::var (static_cast<juce::int64> (*s.seed)) : juce::var() },
        { "patchMode",  s.patchMode ? juce::var (static_cast<int> (*s.patchMode)) : juce::var() },
        { "tempoBpm",   s.tempoBpm ? juce::var (static_cast<int> (*s.tempoBpm)) : juce::var() },
        { "root",       s.tonic ? juce::var (static_cast<int> (*s.tonic)) : juce::var() },
        { "scaleIndex", s.scaleIndex ? juce::var (static_cast<int> (*s.scaleIndex)) : juce::var() },
        { "engines",    engines },
    }));
}

void CompanionEditor::pushLibrary()
{
    if (! pageReady)
        return;

    // The page reads suite envelopes, the same ones on disk -- there is no
    // second shape for the UI to know about.
    web.emit ("library", objectWith ({ { "items", model.library().itemsVar() } }));
}

void CompanionEditor::pushPorts (OnlyIfChanged onlyIfChanged)
{
    if (! pageReady)
        return;

    auto& linkRef = model.link();

    juce::String signature;
    for (const auto& d : linkRef.availableInputs())  signature << d.identifier << ";";
    signature << "|";
    for (const auto& d : linkRef.availableOutputs()) signature << d.identifier << ";";
    signature << "|" << linkRef.inputIdentifier() << "|" << linkRef.outputIdentifier()
              << "|" << (linkRef.isConnected() ? 1 : 0);

    if (onlyIfChanged == OnlyIfChanged::yes && signature == lastPortSignature)
        return;

    lastPortSignature = signature;

    const auto describe = [] (const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        juce::Array<juce::var> out;
        for (const auto& device : devices)
            out.add (objectWith ({ { "id", device.identifier }, { "name", device.name } }));
        return juce::var (out);
    };

    web.emit ("ports", objectWith ({
        { "inputs",      describe (linkRef.availableInputs()) },
        { "outputs",     describe (linkRef.availableOutputs()) },
        { "selectedIn",  linkRef.inputIdentifier() },
        { "selectedOut", linkRef.outputIdentifier() },
        { "connected",   linkRef.isConnected() },
        { "outputName",  linkRef.outputName() },
    }));
}

void CompanionEditor::pushTransport()
{
    if (! pageReady)
        return;

    const auto name = model.transport() == CompanionModel::Transport::host ? "host"
                    : model.transport() == CompanionModel::Transport::both ? "both"
                                                                           : "direct";

    web.emit ("transport", objectWith ({ { "transport", name } }));
}

void CompanionEditor::log (const juce::String& text)
{
    if (pageReady)
        web.emit ("log", objectWith ({ { "text", text } }));
}
