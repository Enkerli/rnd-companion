#pragma once

// Curated seeds, kept on disk as JSON.
//
// Because we do not reimplement the device's generator, an entry's musical
// metadata is whatever the hardware told us while that seed was playing. The
// library is populated by listening, not by computing -- so a seed captured
// without a full status dump is still a valid entry, just a sparser one.

#include "../Protocol/RndProtocol.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <vector>

struct SeedEntry
{
    enum class Rating
    {
        unrated,
        keep,
        pass
    };

    std::uint32_t seed {};
    Rating        rating { Rating::unrated };
    juce::String  note;
    juce::int64   capturedAtMs {};

    // Snapshot of what the device reported, when it reported anything.
    bool          hasStatus {};
    int           patchMode {};
    int           tempoBpm {};
    int           tonic {};
    int           scaleIndex {};
    juce::String  engines;   ///< Comma-separated, in track order.

    juce::String  displayName() const;
    juce::String  summary() const;
};

class SeedLibrary : private juce::Timer
{
public:
    SeedLibrary();
    ~SeedLibrary() override;

    /// Adds the seed, or updates the existing entry for it. Status fields are
    /// only overwritten when `status` actually carries them, so re-capturing a
    /// seed from a bare broadcast never erases a richer earlier snapshot.
    void captureSeed (std::uint32_t seed, const rnd::DeviceStatus& status);

    void setRating (std::uint32_t seed, SeedEntry::Rating rating);
    void setNote   (std::uint32_t seed, const juce::String& note);
    void remove    (std::uint32_t seed);

    const std::vector<SeedEntry>& entries() const noexcept { return items; }
    const SeedEntry* find (std::uint32_t seed) const;
    int indexOf (std::uint32_t seed) const;

    /// Newest first, optionally filtered by rating.
    std::vector<SeedEntry> filtered (bool includeUnrated, bool includeKeep, bool includePass) const;

    bool save() const;
    bool load();

    /// Writes any pending changes now. Called on destruction; call it directly
    /// before anything that must see the file up to date.
    void flush();

    bool exportTo   (const juce::File& file) const;
    bool importFrom (const juce::File& file);

    static juce::File defaultFile();

    std::function<void()> onChanged;

private:
    void timerCallback() override;
    void changed();
    juce::var toVar() const;
    bool fromVar (const juce::var& value, bool merge);

    static bool sameContent (const SeedEntry&, const SeedEntry&);

    std::vector<SeedEntry> items;
    juce::File storage;
    bool dirty = false;
};
