#include "SeedLibrary.h"

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

juce::String SeedEntry::summary() const
{
    if (! hasStatus)
        return "no status captured";

    juce::String text;
    text << juce::String (rnd::tonicName (tonic)) << " " << juce::String (rnd::scaleName (scaleIndex));
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

    if (index >= 0)
        items[static_cast<std::size_t> (index)] = entry;
    else
        items.push_back (entry);

    changed();
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

void SeedLibrary::remove (std::uint32_t seed)
{
    const int index = indexOf (seed);
    if (index < 0)
        return;

    items.erase (items.begin() + index);
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
    juce::Array<juce::var> array;

    for (const auto& entry : items)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("seed", juce::String (rnd::formatSeed (entry.seed)));
        object->setProperty ("rating", ratingToString (entry.rating));
        object->setProperty ("note", entry.note);
        object->setProperty ("capturedAtMs", entry.capturedAtMs);
        object->setProperty ("hasStatus", entry.hasStatus);

        if (entry.hasStatus)
        {
            object->setProperty ("patchMode", entry.patchMode);
            object->setProperty ("tempoBpm", entry.tempoBpm);
            object->setProperty ("tonic", entry.tonic);
            object->setProperty ("scaleIndex", entry.scaleIndex);
            object->setProperty ("engines", entry.engines);
        }

        array.add (juce::var (object));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "rnd-companion-seed-library");
    root->setProperty ("version", 1);
    root->setProperty ("seeds", array);

    return juce::var (root);
}

bool SeedLibrary::fromVar (const juce::var& value, bool merge)
{
    const auto* root = value.getDynamicObject();
    if (root == nullptr)
        return false;

    const auto seeds = root->getProperty ("seeds");
    const auto* array = seeds.getArray();
    if (array == nullptr)
        return false;

    if (! merge)
        items.clear();

    for (const auto& element : *array)
    {
        const auto* object = element.getDynamicObject();
        if (object == nullptr)
            continue;

        const auto parsed = rnd::parseSeed (object->getProperty ("seed").toString().toStdString());
        if (! parsed.has_value())
            continue;

        SeedEntry entry;
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

        const int existing = indexOf (entry.seed);
        if (existing >= 0)
            items[static_cast<std::size_t> (existing)] = entry;
        else
            items.push_back (entry);
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
    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

bool SeedLibrary::importFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    return fromVar (juce::JSON::parse (file.loadFileAsString()), true);
}

void SeedLibrary::changed()
{
    save();

    if (onChanged != nullptr)
        onChanged();
}
