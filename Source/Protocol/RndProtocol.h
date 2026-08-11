#pragma once

// Wire protocol for the Cymaforma RND Synth.
//
// Plain C++17 with no JUCE and no I/O, so the same codec serves the standalone,
// the plugin shells, and a future WASM build of the web UI.
//
// Every device message is  F0 6F 62 78 <cmd> [payload…] F7.  The three bytes
// 6F 62 78 are ASCII "obx", used as a manufacturer tag.  0x6F sits inside the
// MMA-allocated single-byte manufacturer range rather than the 0x7D
// non-commercial slot, so the tag on its own does not prove the sender is an
// RND.  Match the port name as well before trusting a frame.
//
// The vocabulary below is what has been observed, not a published spec.  It
// comes from two sources: the Seed Lab web app's client code
// (https://redteam.fr/seed-lab/), and a capture taken from real hardware
// (Tests/fixtures/CymaRNDfirmUp.mid).  Firmware updates can invalidate any of
// it — see docs/PROTOCOL.md for what is confirmed versus inferred.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rnd
{

// ── Framing ─────────────────────────────────────────────────────────────────

inline constexpr std::uint8_t sysexBegin = 0xF0;
inline constexpr std::uint8_t sysexEnd   = 0xF7;

inline constexpr std::array<std::uint8_t, 3> manufacturerTag { 0x6F, 0x62, 0x78 };

enum class Command : std::uint8_t
{
    seed        = 0x10,  ///< Both directions: the 32-bit patch seed.
    unlock      = 0x11,  ///< Host→device: play-lock = payload[0], then dump status.
    dumpBegin   = 0x20,  ///< Device→host: empty payload, precedes globals+engines.
    globals     = 0x21,  ///< Device→host: patch mode, tempo, tonic, scale.
    trackEngine = 0x22,  ///< Device→host: one per track, carries the engine name.
};

// ── Seeds ───────────────────────────────────────────────────────────────────

/// A 32-bit seed travels as five 7-bit bytes, least-significant septet first.
/// (Leftmost = LSB, which is the suite convention as well — see
/// music-suite/CONVENTIONS.md.)  The fifth byte carries only the top nibble.
using SeedBytes = std::array<std::uint8_t, 5>;

SeedBytes     packSeed (std::uint32_t seed) noexcept;
std::uint32_t unpackSeed (const SeedBytes& bytes) noexcept;

/// "0x0123abcd" — the form the device's seeds are usually written in.
std::string   formatSeed (std::uint32_t seed);

/// Accepts "0x1234abcd", "1234abcd", or a plain decimal integer.
/// Returns nullopt rather than throwing, so UI code can validate as you type.
std::optional<std::uint32_t> parseSeed (const std::string& text);

// ── Decoded messages ────────────────────────────────────────────────────────

struct SeedMessage
{
    std::uint32_t seed {};
};

struct DumpBeginMessage
{
};

struct GlobalsMessage
{
    std::uint8_t  patchMode {};
    std::uint16_t tempoBpm {};    ///< 14-bit, low septet first. See the caveat below.
    std::uint8_t  tonic {};       ///< Pitch class, 0–11.
    std::uint8_t  scaleIndex {};  ///< 0–19, indexes scaleName().
};

struct TrackEngineMessage
{
    std::uint8_t trackIndex {};
    std::uint8_t unknownA {};  ///< Observed 0x00. Purpose unknown; Seed Lab skips it.
    std::uint8_t unknownB {};  ///< Observed 0x01. Purpose unknown; Seed Lab skips it.
    std::string  engineName;   ///< NUL-terminated ASCII on the wire, e.g. "FM".
};

using Message = std::variant<SeedMessage, DumpBeginMessage, GlobalsMessage, TrackEngineMessage>;

/// Decodes one complete SysEx frame.  Returns nullopt when the bytes are not a
/// well-formed RND frame — wrong manufacturer tag, unknown command, truncated
/// payload, or missing terminator.  Never throws, never partially applies.
std::optional<Message> parseSysex (const std::uint8_t* data, std::size_t size);

std::optional<Message> parseSysex (const std::vector<std::uint8_t>& bytes);

// ── Encoded messages (host → device) ────────────────────────────────────────

/// Loads a seed on the device.
std::vector<std::uint8_t> makeSeedMessage (std::uint32_t seed);

/// Clears the play-lock and asks for a status dump.
///
/// This is the only known way to poll the device, and it costs a brief audible
/// mute — so drive it from an explicit user action, never from a timer.  You do
/// not need it to follow the seed: the RND broadcasts an unsolicited `seed`
/// frame whenever its seed changes, which is silent and free.
std::vector<std::uint8_t> makeUnlockAndDump();

// ── Control-change layer ────────────────────────────────────────────────────

namespace cc
{
    inline constexpr std::uint8_t scale  = 9;
    inline constexpr std::uint8_t volume = 7;
    inline constexpr std::uint8_t reverb = 91;
}

/// Volume and reverb are sent to the master (ch 1) plus the per-track takeover
/// band (ch 2–5).  1-based, as MIDI channels are spoken about.
inline constexpr std::array<int, 5> mixChannels { 1, 2, 3, 4, 5 };

inline constexpr int numScales = 20;
inline constexpr int numTonics = 12;

/// The RND splits CC9's 0–127 range into 20 scale bands.  This returns the
/// midpoint of a band, which is what you send to select a scale reliably.
/// Equivalent to floor(3.2 + 6.4 * index), but the table is authoritative.
std::uint8_t scaleCcValue (int scaleIndex);

/// Inverse of scaleCcValue: which scale a given CC9 value selects.
int scaleIndexForCc (std::uint8_t value);

/// Tonic is set by pulsing a note on channel 1, not by a CC.
inline constexpr int tonicNoteBase = 60;
int tonicNoteNumber (int pitchClass);

/// Scale and tonic *lock* on the hardware once set: the same seed will then
/// produce different engines, and only a power cycle is known to clear it.
/// Nothing in the observed vocabulary clears the lock — `unlock` (0x11 0x00)
/// addresses the play-lock, which appears to be a different thing.
inline constexpr bool scaleAndTonicLockOnDevice = true;

// ── Names ───────────────────────────────────────────────────────────────────

/// Chromatic names, deliberately.  A tonic byte arrives as a bare pitch class
/// with no chord or scale context to spell it from, and the suite convention
/// says bare pitch-class data stays chromatic.  Do not "fix" this toward
/// structural spelling — see music-suite/CONVENTIONS.md.
const char* tonicName (int pitchClass);

/// Index order is the device's, mirrored from Seed Lab's table.
const char* scaleName (int scaleIndex);

// ── Accumulated device state ────────────────────────────────────────────────

struct TrackEngine
{
    std::uint8_t index {};
    std::string  name;
};

/// Everything the device has told us about what it is currently playing.
///
/// A dump arrives as several frames in a row, so this folds them together.
/// Fields stay unset until the device actually reports them, which is why they
/// are optional: an unsolicited seed broadcast tells you the seed and nothing
/// else, and showing a stale tempo next to a fresh seed would be a lie.
struct DeviceStatus
{
    std::optional<std::uint32_t> seed;
    std::optional<std::uint8_t>  patchMode;
    std::optional<std::uint16_t> tempoBpm;
    std::optional<std::uint8_t>  tonic;
    std::optional<std::uint8_t>  scaleIndex;
    std::vector<TrackEngine>     engines;  ///< Sorted by track index.

    /// Folds one decoded frame in.  A `seed` or `dumpBegin` frame clears the
    /// engine list, because both mean a new patch is being described and the
    /// old engine names no longer apply.
    void apply (const Message& message);

    void clear();

    /// True once we know at least the seed.
    bool hasSeed() const noexcept { return seed.has_value(); }
};

// The tempo field is reported in BPM and Seed Lab treats it as such, but a
// capture at 125 BPM had a note grid whose pulse does not divide evenly into
// that tempo (roughly a 4:5 relationship).  Treat tempoBpm as "what the device
// says" and calibrate before driving MIDI clock from it.

}  // namespace rnd
