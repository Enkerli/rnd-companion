#pragma once

// A deliberately small Standard MIDI File reader — just enough to replay a
// hardware capture through the protocol codec in tests.  It is not a general
// SMF library and should never grow into one: the standalone and the plugins
// read live MIDI through JUCE, not through this.

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rndtest
{

struct NoteEvent
{
    std::uint32_t tick {};
    int           channel {};   ///< 1-based.
    int           note {};
    int           velocity {};
};

struct SysexEvent
{
    std::uint32_t             tick {};
    std::vector<std::uint8_t> bytes;  ///< Includes the leading F0 and trailing F7.
};

struct Capture
{
    int                     format {};
    int                     numTracks {};
    int                     ticksPerQuarter {};
    std::vector<NoteEvent>  noteOns;
    std::vector<SysexEvent> sysex;
};

inline Capture readSmf (const std::string& path)
{
    std::ifstream stream (path, std::ios::binary);
    if (! stream)
        throw std::runtime_error ("cannot open " + path);

    const std::vector<std::uint8_t> data ((std::istreambuf_iterator<char> (stream)),
                                          std::istreambuf_iterator<char>());

    std::size_t pos = 0;

    auto need = [&] (std::size_t n)
    {
        if (pos + n > data.size())
            throw std::runtime_error ("truncated MIDI file");
    };

    auto u32 = [&]
    {
        need (4);
        const std::uint32_t v = (static_cast<std::uint32_t> (data[pos]) << 24)
                              | (static_cast<std::uint32_t> (data[pos + 1]) << 16)
                              | (static_cast<std::uint32_t> (data[pos + 2]) << 8)
                              | static_cast<std::uint32_t> (data[pos + 3]);
            pos += 4;
        return v;
    };

    auto u16 = [&]
    {
        need (2);
        const std::uint16_t v = static_cast<std::uint16_t> ((data[pos] << 8) | data[pos + 1]);
        pos += 2;
        return v;
    };

    auto vlq = [&]
    {
        std::uint32_t v = 0;
        for (;;)
        {
            need (1);
            const std::uint8_t b = data[pos++];
            v = (v << 7) | static_cast<std::uint32_t> (b & 0x7F);
            if ((b & 0x80) == 0)
                return v;
        }
    };

    need (4);
    if (! (data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd'))
        throw std::runtime_error ("not a MIDI file");
    pos = 4;

    const std::uint32_t headerLength = u32();
    const std::size_t   headerEnd = pos + headerLength;

    Capture capture;
    capture.format          = u16();
    capture.numTracks       = u16();
    capture.ticksPerQuarter = u16();
    pos = headerEnd;

    for (int track = 0; track < capture.numTracks; ++track)
    {
        need (4);
        if (! (data[pos] == 'M' && data[pos + 1] == 'T' && data[pos + 2] == 'r' && data[pos + 3] == 'k'))
            throw std::runtime_error ("expected MTrk");
        pos += 4;

        const std::uint32_t trackLength = u32();
        const std::size_t   trackEnd = pos + trackLength;

        std::uint32_t tick = 0;
        std::uint8_t  runningStatus = 0;

        while (pos < trackEnd)
        {
            tick += vlq();
            need (1);
            const std::uint8_t next = data[pos];

            if (next == 0xFF)
            {
                pos += 1;
                need (1);
                pos += 1;  // meta type
                const std::uint32_t length = vlq();
                need (length);
                pos += length;
                continue;
            }

            if (next == 0xF0 || next == 0xF7)
            {
                pos += 1;
                const std::uint32_t length = vlq();
                need (length);

                SysexEvent event;
                event.tick = tick;
                if (next == 0xF0)
                    event.bytes.push_back (0xF0);
                event.bytes.insert (event.bytes.end(), data.begin() + static_cast<long> (pos),
                                    data.begin() + static_cast<long> (pos + length));
                pos += length;
                capture.sysex.push_back (std::move (event));
                continue;
            }

            if ((next & 0x80) != 0)
            {
                runningStatus = next;
                pos += 1;
            }

            const std::uint8_t status = runningStatus;
            const std::uint8_t kind = status & 0xF0;
            const int channel = (status & 0x0F) + 1;
            const std::size_t numDataBytes = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;

            need (numDataBytes);
            const std::uint8_t d0 = data[pos];
            const std::uint8_t d1 = numDataBytes > 1 ? data[pos + 1] : 0;
            pos += numDataBytes;

            if (kind == 0x90 && d1 > 0)
                capture.noteOns.push_back ({ tick, channel, d0, d1 });
        }

        pos = trackEnd;
    }

    return capture;
}

}  // namespace rndtest
