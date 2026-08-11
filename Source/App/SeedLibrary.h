#pragma once

// Curated seeds, kept on disk as suite library items.
//
// Because we do not reimplement the device's generator, an entry's musical
// metadata is whatever the hardware told us while that seed was playing. The
// library is populated by listening, not by computing -- so a seed captured
// without a full status dump is still a valid entry, just a sparser one.
//
// On disk each seed is an `enkerli-library-item` envelope (the suite's
// LIBRARY_SPEC): kind `patch`, format `rnd-seed`, with the RND-specific part
// carried verbatim as the payload. That buys stable identity, provenance, and
// facets a person can actually search -- "dorian", not scale index 6 -- and it
// means a library exported here opens in the rest of the suite. The TypeScript
// twin is @enkerli/rnd/library, tested against the suite's own validator.
//
// Files written by the earlier private format are migrated on load.

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

    /// Envelope identity. Stable across saves, never derived from the seed --
    /// LIBRARY_SPEC forbids deriving the id from the title.
    juce::String  id;
    /// Absolute ISO 8601, seconds precision. Never a file-system date.
    juce::String  savedAt;

    // Snapshot of what the device reported, when it reported anything.
    bool          hasStatus {};
    int           patchMode {};
    int           tempoBpm {};
    int           tonic {};
    int           scaleIndex {};
    juce::String  engines;   ///< Comma-separated, in track order.

    juce::String  displayName() const;
    juce::String  summary() const;

    /// The shortest true description: root and scale, nothing else. Tempo and
    /// engines live in summary(), which the row carries as its spoken name.
    juce::String  shortDescription() const;

    /// True once the device has told us what this seed sounds like.
    bool hasFullCapture() const noexcept { return hasStatus; }
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
    /// Returns what was removed, so the caller can offer an undo. Deleting a
    /// curated seed is the one destructive act in this app; it does not happen
    /// without a way back.
    std::optional<SeedEntry> remove (std::uint32_t seed);

    /// Puts a removed entry back exactly as it was.
    void reinsert (const SeedEntry&);

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

    /// The entries as suite envelope items. The UI reads exactly the shape the
    /// file holds -- there is no second form for the view to know about.
    juce::var itemsVar() const;

    /// Serialised form, for transports that hand over bytes rather than paths
    /// (the iOS share sheet, say).
    juce::String toJsonString() const;
    bool importJsonString (const juce::String&);

    /// Export names carry the moment they were taken: without it a folder of
    /// exports is a pile of rnd-seeds2, rnd-seeds3, rnd-seeds4 with no way to
    /// tell which is newest.
    static juce::String timestampedExportName();

    static juce::File defaultFile();

    std::function<void()> onChanged;

private:
    void timerCallback() override;
    void changed();
    juce::var toVar() const;
    bool fromVar (const juce::var& value, bool merge);

    static bool sameContent (const SeedEntry&, const SeedEntry&);
    static bool readEnvelopeItem (const juce::var&, SeedEntry&);
    static bool readLegacyItem (const juce::var&, SeedEntry&);

    std::vector<SeedEntry> items;
    juce::File storage;
    bool dirty = false;
};
