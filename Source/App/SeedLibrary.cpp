#include "SeedLibrary.h"

#include "BuildStamp.h"


namespace
{
    // ── The suite envelope (LIBRARY_SPEC) ────────────────────────────────────
    // Mirrored by @enkerli/rnd/library in the monorepo, which is tested against
    // the suite's own validateEnvelope. Change one, change both.

    constexpr const char* envelopeName    = "enkerli-library-item";
    constexpr int         envelopeVersion = 1;
    constexpr const char* seedFormat      = "rnd-seed";
    constexpr int         seedFormatVersion = 1;
    constexpr const char* appId           = "rnd-companion";

    juce::String newItemId()
    {
        return juce::Uuid().toDashedString();
    }

    /// Absolute ISO 8601, seconds precision -- LIBRARY_SPEC's preservation rule.
    juce::String isoNow()
    {
        return juce::Time::getCurrentTime().toISO8601 (true).upToFirstOccurrenceOf (".", false, false) + "Z";
    }

    juce::String isoFromMillis (juce::int64 ms)
    {
        return juce::Time (ms).toISO8601 (true).upToFirstOccurrenceOf (".", false, false) + "Z";
    }
}

namespace
{
    const char* ratingToString (SeedEntry::Rating rating)
    {
        switch (rating)
        {
            case SeedEntry::Rating::keep: return "keep";
            case SeedEntry::Rating::pass: return "pass";
            case SeedEntry::Rating::unrated: break;
        }
        return "unrated";
    }

    SeedEntry::Rating ratingFromString (const juce::String& text)
    {
        if (text == "keep") return SeedEntry::Rating::keep;
        if (text == "pass") return SeedEntry::Rating::pass;
        return SeedEntry::Rating::unrated;
    }
}

//==============================================================================
juce::String SeedEntry::displayName() const
{
    return juce::String (rnd::formatSeed (seed));
}

juce::String SeedEntry::shortDescription() const
{
    if (! hasStatus)
        return "no status captured";

    return juce::String (rnd::tonicName (tonic)) + " " + juce::String (rnd::scaleName (scaleIndex));
}

juce::String SeedEntry::summary() const
{
    if (! hasStatus)
        return "no status captured";

    juce::String text;
    // "root when captured", not the seed's tonic: the device reports the root
    // it is playing now, and that moves while the patch runs.
    text << juce::String (rnd::scaleName (scaleIndex)) << ", root " << juce::String (rnd::tonicName (tonic)) << " when captured";
    text << ", " << tempoBpm << " BPM";

    if (engines.isNotEmpty())
        text << ", " << engines;

    return text;
}

//==============================================================================
SeedLibrary::SeedLibrary()
    : storage (defaultFile())
{
}

SeedLibrary::~SeedLibrary()
{
    stopTimer();
    flush();
}

juce::File SeedLibrary::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("RND Companion")
               .getChildFile ("seed-library.json");
}

int SeedLibrary::indexOf (std::uint32_t seed) const
{
    for (int i = 0; i < static_cast<int> (items.size()); ++i)
        if (items[static_cast<std::size_t> (i)].seed == seed)
            return i;

    return -1;
}

const SeedEntry* SeedLibrary::find (std::uint32_t seed) const
{
    const int index = indexOf (seed);
    return index >= 0 ? &items[static_cast<std::size_t> (index)] : nullptr;
}

void SeedLibrary::captureSeed (std::uint32_t seed, const rnd::DeviceStatus& status)
{
    const int index = indexOf (seed);

    SeedEntry entry;
    if (index >= 0)
        entry = items[static_cast<std::size_t> (index)];
    else
        entry.seed = seed;

    entry.capturedAtMs = juce::Time::getCurrentTime().toMilliseconds();

    if (entry.id.isEmpty())
        entry.id = newItemId();

    entry.savedAt = isoNow();

    // Only overwrite the musical snapshot when the device has actually told us
    // all of it. A seed broadcast on its own carries no globals, and half a
    // status next to a seed is worse than none.
    if (status.tempoBpm && status.tonic && status.scaleIndex && status.patchMode)
    {
        entry.hasStatus  = true;
        entry.patchMode  = *status.patchMode;
        entry.tempoBpm   = *status.tempoBpm;
        entry.tonic      = *status.tonic;
        entry.scaleIndex = *status.scaleIndex;

        juce::StringArray names;
        for (const auto& engine : status.engines)
            names.add (juce::String (engine.name));

        entry.engines = names.joinIntoString (", ");
    }

    // The device repeats its status constantly, so the same capture arrives
    // over and over. Writing an identical entry would churn the file for
    // nothing.
    if (index >= 0 && sameContent (items[static_cast<std::size_t> (index)], entry))
        return;

    if (index >= 0)
        items[static_cast<std::size_t> (index)] = entry;
    else
        items.push_back (entry);

    changed();
}

bool SeedLibrary::sameContent (const SeedEntry& a, const SeedEntry& b)
{
    // capturedAtMs, id and savedAt deliberately excluded: a re-capture of
    // unchanged content is not a change worth persisting.
    return a.seed == b.seed
        && a.rating == b.rating
        && a.note == b.note
        && a.hasStatus == b.hasStatus
        && a.patchMode == b.patchMode
        && a.tempoBpm == b.tempoBpm
        && a.tonic == b.tonic
        && a.scaleIndex == b.scaleIndex
        && a.engines == b.engines;
}

void SeedLibrary::setRating (std::uint32_t seed, SeedEntry::Rating rating)
{
    const int index = indexOf (seed);
    if (index < 0)
        return;

    items[static_cast<std::size_t> (index)].rating = rating;
    changed();
}

void SeedLibrary::setNote (std::uint32_t seed, const juce::String& note)
{
    const int index = indexOf (seed);
    if (index < 0)
        return;

    items[static_cast<std::size_t> (index)].note = note;
    changed();
}

std::optional<SeedEntry> SeedLibrary::remove (std::uint32_t seed)
{
    const int index = indexOf (seed);
    if (index < 0)
        return std::nullopt;

    const SeedEntry removed = items[static_cast<std::size_t> (index)];
    items.erase (items.begin() + index);
    changed();
    return removed;
}

void SeedLibrary::reinsert (const SeedEntry& entry)
{
    if (indexOf (entry.seed) >= 0)
        return;

    items.push_back (entry);
    changed();
}

std::vector<SeedEntry> SeedLibrary::filtered (bool includeUnrated, bool includeKeep, bool includePass) const
{
    std::vector<SeedEntry> result;

    for (const auto& entry : items)
    {
        const bool wanted = (entry.rating == SeedEntry::Rating::unrated && includeUnrated)
                         || (entry.rating == SeedEntry::Rating::keep && includeKeep)
                         || (entry.rating == SeedEntry::Rating::pass && includePass);

        if (wanted)
            result.push_back (entry);
    }

    std::sort (result.begin(), result.end(),
               [] (const SeedEntry& a, const SeedEntry& b) { return a.capturedAtMs > b.capturedAtMs; });

    return result;
}

//==============================================================================
juce::var SeedLibrary::toVar() const
{
    juce::Array<juce::var> itemVars;   // not `items`: that is the member

    for (const auto& entry : items)
    {
        auto* payload = new juce::DynamicObject();
        payload->setProperty ("seed", entry.displayName());
        payload->setProperty ("rating", ratingToString (entry.rating));
        if (entry.note.isNotEmpty())
            payload->setProperty ("note", entry.note);

        auto* facets = new juce::DynamicObject();
        facets->setProperty ("seedValue", static_cast<juce::int64> (entry.seed));
        facets->setProperty ("rating", ratingToString (entry.rating));
        facets->setProperty ("hasStatus", entry.hasStatus);

        juce::StringArray tags;
        tags.add (ratingToString (entry.rating));

        if (entry.hasStatus)
        {
            // The RND-specific part, carried verbatim: adoption means wrapping,
            // not migrating.
            auto* captured = new juce::DynamicObject();
            captured->setProperty ("patchMode", entry.patchMode);
            captured->setProperty ("tempoBpm", entry.tempoBpm);
            // Named for what it is. The device reports the root it is playing
            // now, and that moves while the patch runs.
            captured->setProperty ("rootWhenCaptured", entry.tonic);
            captured->setProperty ("scaleIndex", entry.scaleIndex);

            juce::Array<juce::var> engines;
            for (const auto& name : juce::StringArray::fromTokens (entry.engines, ",", ""))
                engines.add (name.trim());
            captured->setProperty ("engines", engines);

            payload->setProperty ("captured", juce::var (captured));

            // Facets are what makes a library searchable, so the human-readable
            // names go in too: someone looking for "dorian" should not have to
            // know it is 6.
            facets->setProperty ("tempoBpm", entry.tempoBpm);
            facets->setProperty ("patchMode", entry.patchMode);
            facets->setProperty ("scaleIndex", entry.scaleIndex);
            facets->setProperty ("scale", juce::String (rnd::scaleName (entry.scaleIndex)));
            facets->setProperty ("rootWhenCaptured", entry.tonic);
            facets->setProperty ("rootName", juce::String (rnd::tonicName (entry.tonic)));
            facets->setProperty ("trackCount", engines.size());
            facets->setProperty ("engines", entry.engines);

            tags.add (juce::String (rnd::scaleName (entry.scaleIndex)));
        }

        auto* generator = new juce::DynamicObject();
        generator->setProperty ("app", appId);
        generator->setProperty ("version", RND_BUILD_STAMP);

        auto* provenance = new juce::DynamicObject();
        provenance->setProperty ("generator", juce::var (generator));
        provenance->setProperty ("license", "Unlicense");

        auto* item = new juce::DynamicObject();
        item->setProperty ("envelope", envelopeName);
        item->setProperty ("envelopeVersion", envelopeVersion);
        item->setProperty ("id", entry.id.isNotEmpty() ? entry.id : newItemId());
        item->setProperty ("kind", "patch");
        item->setProperty ("format", seedFormat);
        item->setProperty ("formatVersion", seedFormatVersion);
        item->setProperty ("title", entry.displayName());
        item->setProperty ("app", appId);
        item->setProperty ("savedAt", entry.savedAt.isNotEmpty() ? entry.savedAt : isoFromMillis (entry.capturedAtMs));
        item->setProperty ("provenance", juce::var (provenance));

        juce::Array<juce::var> tagVars;
        for (const auto& t : tags) tagVars.add (t);
        item->setProperty ("tags", tagVars);

        item->setProperty ("facets", juce::var (facets));
        item->setProperty ("payload", juce::var (payload));

        itemVars.add (juce::var (item));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("envelope", "enkerli-library-index");
    root->setProperty ("envelopeVersion", envelopeVersion);
    root->setProperty ("app", appId);
    root->setProperty ("items", itemVars);

    return juce::var (root);
}

bool SeedLibrary::readEnvelopeItem (const juce::var& value, SeedEntry& entry)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    if (object->getProperty ("format").toString() != seedFormat)
        return false;

    const auto* payload = object->getProperty ("payload").getDynamicObject();
    if (payload == nullptr)
        return false;

    const auto parsed = rnd::parseSeed (payload->getProperty ("seed").toString().toStdString());
    if (! parsed.has_value())
        return false;

    entry = SeedEntry {};
    entry.seed    = *parsed;
    entry.rating  = ratingFromString (payload->getProperty ("rating").toString());
    entry.note    = payload->getProperty ("note").toString();
    entry.id      = object->getProperty ("id").toString();
    entry.savedAt = object->getProperty ("savedAt").toString();

    // Sort order still uses a millisecond stamp; derive it from savedAt so an
    // imported library keeps its ordering.
    juce::int64 ms = 0;
    if (entry.savedAt.isNotEmpty())
    {
        const auto parsedTime = juce::Time::fromISO8601 (entry.savedAt);
        ms = parsedTime.toMilliseconds();
    }
    entry.capturedAtMs = ms != 0 ? ms : juce::Time::getCurrentTime().toMilliseconds();

    if (const auto* captured = payload->getProperty ("captured").getDynamicObject())
    {
        entry.hasStatus  = true;
        entry.patchMode  = static_cast<int> (captured->getProperty ("patchMode"));
        entry.tempoBpm   = static_cast<int> (captured->getProperty ("tempoBpm"));
        entry.tonic      = static_cast<int> (captured->getProperty ("rootWhenCaptured"));
        entry.scaleIndex = static_cast<int> (captured->getProperty ("scaleIndex"));

        juce::StringArray names;
        if (const auto* engines = captured->getProperty ("engines").getArray())
            for (const auto& e : *engines)
                names.add (e.toString());

        entry.engines = names.joinIntoString (", ");
    }

    return true;
}

bool SeedLibrary::readLegacyItem (const juce::var& value, SeedEntry& entry)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    const auto parsed = rnd::parseSeed (object->getProperty ("seed").toString().toStdString());
    if (! parsed.has_value())
        return false;

    entry = SeedEntry {};
    entry.seed         = *parsed;
    entry.rating       = ratingFromString (object->getProperty ("rating").toString());
    entry.note         = object->getProperty ("note").toString();
    entry.capturedAtMs = static_cast<juce::int64> (object->getProperty ("capturedAtMs"));
    entry.hasStatus    = static_cast<bool> (object->getProperty ("hasStatus"));

    if (entry.hasStatus)
    {
        entry.patchMode  = static_cast<int> (object->getProperty ("patchMode"));
        entry.tempoBpm   = static_cast<int> (object->getProperty ("tempoBpm"));
        entry.tonic      = static_cast<int> (object->getProperty ("tonic"));
        entry.scaleIndex = static_cast<int> (object->getProperty ("scaleIndex"));
        entry.engines    = object->getProperty ("engines").toString();
    }

    // Migrated entries get identity now, so they stop being anonymous the
    // moment they are read.
    entry.id      = newItemId();
    entry.savedAt = isoFromMillis (entry.capturedAtMs);
    return true;
}

bool SeedLibrary::fromVar (const juce::var& value, bool merge)
{
    const auto* root = value.getDynamicObject();
    if (root == nullptr)
        return false;

    // The envelope form lists `items`; the private format we used to write
    // listed `seeds`. Read either, always write the former.
    const bool legacy = root->hasProperty ("seeds") && ! root->hasProperty ("items");
    const auto* array = root->getProperty (legacy ? "seeds" : "items").getArray();
    if (array == nullptr)
        return false;

    if (! merge)
        items.clear();

    int migrated = 0;

    for (const auto& element : *array)
    {
        SeedEntry entry;
        const bool read = legacy ? readLegacyItem (element, entry) : readEnvelopeItem (element, entry);
        if (! read)
            continue;

        if (legacy)
            ++migrated;

        const int existing = indexOf (entry.seed);
        if (existing >= 0)
            items[static_cast<std::size_t> (existing)] = entry;
        else
            items.push_back (entry);
    }

    if (migrated > 0)
    {
        // Rewrite in the envelope form straight away, so the migration happens
        // once rather than on every load.
        dirty = true;
        flush();
    }

    changed();
    return true;
}

bool SeedLibrary::save() const
{
    storage.getParentDirectory().createDirectory();
    return storage.replaceWithText (juce::JSON::toString (toVar(), false));
}

bool SeedLibrary::load()
{
    if (! storage.existsAsFile())
        return false;

    return fromVar (juce::JSON::parse (storage.loadFileAsString()), false);
}

bool SeedLibrary::exportTo (const juce::File& file) const
{
    return file.replaceWithText (toJsonString());
}

juce::String SeedLibrary::toJsonString() const
{
    return juce::JSON::toString (toVar(), false);
}

bool SeedLibrary::importJsonString (const juce::String& text)
{
    return fromVar (juce::JSON::parse (text), true);
}

juce::String SeedLibrary::timestampedExportName()
{
    return "rnd-seeds-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d-%H%M%S") + ".json";
}

bool SeedLibrary::importFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    return fromVar (juce::JSON::parse (file.loadFileAsString()), true);
}

void SeedLibrary::changed()
{
    // Coalesce writes rather than hitting the disk per edit: a burst of changes
    // (an import, or a device that will not stop talking) becomes one save.
    dirty = true;
    startTimer (1500);

    if (onChanged != nullptr)
        onChanged();
}

void SeedLibrary::timerCallback()
{
    stopTimer();
    flush();
}

void SeedLibrary::flush()
{
    if (! dirty)
        return;

    dirty = false;
    save();
}
