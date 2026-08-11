#include "RndProtocol.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace rnd
{

// ── Seeds ───────────────────────────────────────────────────────────────────

SeedBytes packSeed (std::uint32_t seed) noexcept
{
    return { static_cast<std::uint8_t> (seed         & 0x7F),
             static_cast<std::uint8_t> ((seed >>  7) & 0x7F),
             static_cast<std::uint8_t> ((seed >> 14) & 0x7F),
             static_cast<std::uint8_t> ((seed >> 21) & 0x7F),
             static_cast<std::uint8_t> ((seed >> 28) & 0x0F) };
}

std::uint32_t unpackSeed (const SeedBytes& b) noexcept
{
    return static_cast<std::uint32_t> (b[0] & 0x7F)
         | (static_cast<std::uint32_t> (b[1] & 0x7F) <<  7)
         | (static_cast<std::uint32_t> (b[2] & 0x7F) << 14)
         | (static_cast<std::uint32_t> (b[3] & 0x7F) << 21)
         | (static_cast<std::uint32_t> (b[4] & 0x0F) << 28);
}

std::string formatSeed (std::uint32_t seed)
{
    char buffer[11] {};
    std::snprintf (buffer, sizeof (buffer), "0x%08x", seed);
    return std::string (buffer);
}

std::optional<std::uint32_t> parseSeed (const std::string& text)
{
    // Trim, then accept 0x-prefixed hex, bare hex, or decimal.  Bare hex is
    // only assumed when the string cannot be a plausible decimal, so that
    // typing "125" means 125 rather than 0x125.
    auto begin = text.find_first_not_of (" \t\r\n");
    if (begin == std::string::npos)
        return std::nullopt;

    auto end = text.find_last_not_of (" \t\r\n");
    std::string s = text.substr (begin, end - begin + 1);

    std::transform (s.begin(), s.end(), s.begin(),
                    [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });

    int base = 10;
    if (s.rfind ("0x", 0) == 0)
    {
        s = s.substr (2);
        base = 16;
    }
    else if (s.find_first_not_of ("0123456789") != std::string::npos)
    {
        base = 16;
    }

    if (s.empty() || s.find_first_not_of ("0123456789abcdef") != std::string::npos)
        return std::nullopt;

    if (base == 10 && s.size() > 10)
        return std::nullopt;
    if (base == 16 && s.size() > 8)
        return std::nullopt;

    const auto value = std::strtoull (s.c_str(), nullptr, base);
    if (value > 0xFFFFFFFFull)
        return std::nullopt;

    return static_cast<std::uint32_t> (value);
}

// ── Parsing ─────────────────────────────────────────────────────────────────

std::optional<Message> parseSysex (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr)
        return std::nullopt;

    // Some hosts and drivers hand over the frame without the leading F0, the
    // trailing F7, or both.  Normalise to the payload between them.
    std::size_t first = 0;
    if (size > 0 && data[0] == sysexBegin)
        first = 1;

    std::size_t last = size;  // one past the end of the body
    if (last > first && data[last - 1] == sysexEnd)
        --last;

    if (last <= first)
        return std::nullopt;

    const std::uint8_t* body = data + first;
    const std::size_t   bodySize = last - first;

    // manufacturer tag (3) + command (1)
    if (bodySize < manufacturerTag.size() + 1)
        return std::nullopt;

    if (! std::equal (manufacturerTag.begin(), manufacturerTag.end(), body))
        return std::nullopt;

    const auto command = static_cast<Command> (body[manufacturerTag.size()]);
    const std::uint8_t* payload = body + manufacturerTag.size() + 1;
    const std::size_t   payloadSize = bodySize - manufacturerTag.size() - 1;

    switch (command)
    {
        case Command::seed:
        {
            if (payloadSize < 5)
                return std::nullopt;

            SeedBytes bytes {};
            std::copy_n (payload, bytes.size(), bytes.begin());
            return Message { SeedMessage { unpackSeed (bytes) } };
        }

        case Command::dumpBegin:
        {
            if (payloadSize != 0)
                return std::nullopt;

            return Message { DumpBeginMessage {} };
        }

        case Command::globals:
        {
            if (payloadSize < 5)
                return std::nullopt;

            GlobalsMessage message;
            message.patchMode  = payload[0];
            message.tempoBpm   = static_cast<std::uint16_t> ((payload[1] & 0x7F)
                                                             | ((payload[2] & 0x7F) << 7));
            message.tonic      = payload[3];
            message.scaleIndex = payload[4];
            return Message { message };
        }

        case Command::trackEngine:
        {
            if (payloadSize < 3)
                return std::nullopt;

            TrackEngineMessage message;
            message.trackIndex = payload[0];
            message.unknownA   = payload[1];
            message.unknownB   = payload[2];

            for (std::size_t i = 3; i < payloadSize; ++i)
            {
                if (payload[i] == 0)
                    break;

                message.engineName.push_back (static_cast<char> (payload[i]));
            }

            return Message { std::move (message) };
        }

        case Command::unlock:
            // Host→device only. If we see it we are watching our own output
            // echoed back, which is not an error but carries no device state.
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Message> parseSysex (const std::vector<std::uint8_t>& bytes)
{
    return parseSysex (bytes.data(), bytes.size());
}

namespace
{
    /// Skips an optional leading F0 and reports what is left.
    const std::uint8_t* bodyOf (const std::uint8_t* data, std::size_t size, std::size_t& bodySize)
    {
        if (data == nullptr || size == 0)
        {
            bodySize = 0;
            return nullptr;
        }

        const std::size_t offset = (data[0] == sysexBegin) ? 1u : 0u;
        bodySize = size - offset;
        return data + offset;
    }
}

bool hasManufacturerTag (const std::uint8_t* data, std::size_t size)
{
    std::size_t bodySize = 0;
    const auto* body = bodyOf (data, size, bodySize);

    if (body == nullptr || bodySize < manufacturerTag.size())
        return false;

    return std::equal (manufacturerTag.begin(), manufacturerTag.end(), body);
}

bool hasManufacturerTag (const std::vector<std::uint8_t>& bytes)
{
    return hasManufacturerTag (bytes.data(), bytes.size());
}

std::string describeForeignSysex (const std::uint8_t* data, std::size_t size)
{
    if (hasManufacturerTag (data, size))
        return {};

    std::size_t bodySize = 0;
    const auto* body = bodyOf (data, size, bodySize);

    if (body == nullptr || bodySize == 0)
        return "empty SysEx";

    switch (body[0])
    {
        case 0x7E: return "universal non-real-time SysEx";
        case 0x7F: return "universal real-time SysEx";
        case 0x7D: return "non-commercial SysEx";
        default: break;
    }

    char buffer[32] {};
    std::snprintf (buffer, sizeof (buffer), "manufacturer 0x%02X SysEx", body[0]);
    return std::string (buffer);
}

// ── Encoding ────────────────────────────────────────────────────────────────

static std::vector<std::uint8_t> frame (Command command,
                                        std::initializer_list<std::uint8_t> payload)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve (manufacturerTag.size() + payload.size() + 3);

    bytes.push_back (sysexBegin);
    bytes.insert (bytes.end(), manufacturerTag.begin(), manufacturerTag.end());
    bytes.push_back (static_cast<std::uint8_t> (command));
    bytes.insert (bytes.end(), payload.begin(), payload.end());
    bytes.push_back (sysexEnd);

    return bytes;
}

std::vector<std::uint8_t> makeSeedMessage (std::uint32_t seed)
{
    const auto packed = packSeed (seed);
    return frame (Command::seed, { packed[0], packed[1], packed[2], packed[3], packed[4] });
}

std::vector<std::uint8_t> makeUnlockAndDump()
{
    return frame (Command::unlock, { 0x00 });
}

// ── Control-change layer ────────────────────────────────────────────────────

namespace
{
    // The device's own CC9 band midpoints, mirrored from Seed Lab.
    constexpr std::array<std::uint8_t, numScales> scaleCcMidpoints {
        3, 9, 16, 22, 28, 35, 41, 48, 54, 60, 67, 73, 80, 86, 92, 99, 105, 112, 118, 124
    };

    constexpr std::array<const char*, numTonics> tonicNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    constexpr std::array<const char*, numScales> scaleNames {
        "major",
        "minor",
        "harmonic minor",
        "blues",
        "major pentatonic",
        "minor pentatonic",
        "dorian",
        "phrygian",
        "lydian",
        "mixolydian",
        "locrian",
        "whole tone",
        "double harmonic",
        "hungarian minor",
        "phrygian dominant",
        "hirajoshi",
        "insen",
        "prometheus",
        "octatonic (WT/HT)",
        "persian"
    };
}

std::uint8_t scaleCcValue (int scaleIndex)
{
    if (scaleIndex < 0 || scaleIndex >= numScales)
        return scaleCcMidpoints[0];

    return scaleCcMidpoints[static_cast<std::size_t> (scaleIndex)];
}

int scaleIndexForCc (std::uint8_t value)
{
    const int index = (static_cast<int> (value & 0x7F) * numScales) / 128;
    return std::min (index, numScales - 1);
}

int tonicNoteNumber (int pitchClass)
{
    const int pc = ((pitchClass % numTonics) + numTonics) % numTonics;
    return tonicNoteBase + pc;
}

const char* tonicName (int pitchClass)
{
    if (pitchClass < 0 || pitchClass >= numTonics)
        return "?";

    return tonicNames[static_cast<std::size_t> (pitchClass)];
}

const char* scaleName (int scaleIndex)
{
    if (scaleIndex < 0 || scaleIndex >= numScales)
        return "?";

    return scaleNames[static_cast<std::size_t> (scaleIndex)];
}

// ── DeviceStatus ────────────────────────────────────────────────────────────

void DeviceStatus::clear()
{
    *this = DeviceStatus {};
}

void DeviceStatus::apply (const Message& message)
{
    if (const auto* m = std::get_if<SeedMessage> (&message))
    {
        seed = m->seed;
        engines.clear();
        return;
    }

    if (std::get_if<DumpBeginMessage> (&message) != nullptr)
    {
        engines.clear();
        return;
    }

    if (const auto* m = std::get_if<GlobalsMessage> (&message))
    {
        patchMode  = m->patchMode;
        tempoBpm   = m->tempoBpm;
        tonic      = m->tonic;
        scaleIndex = m->scaleIndex;
        return;
    }

    if (const auto* m = std::get_if<TrackEngineMessage> (&message))
    {
        const auto existing = std::find_if (engines.begin(), engines.end(),
                                            [m] (const TrackEngine& e) { return e.index == m->trackIndex; });

        if (existing != engines.end())
            existing->name = m->engineName;
        else
            engines.push_back ({ m->trackIndex, m->engineName });

        std::sort (engines.begin(), engines.end(),
                   [] (const TrackEngine& a, const TrackEngine& b) { return a.index < b.index; });
    }
}

}  // namespace rnd
