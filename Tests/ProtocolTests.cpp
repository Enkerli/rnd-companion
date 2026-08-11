// Protocol codec tests.
//
// The interesting ones at the bottom replay a real hardware capture
// (fixtures/CymaRNDfirmUp.mid, recorded in Logic Pro from an RND Synth) through
// the codec.  That capture is the only ground truth we have for this protocol,
// so it is checked byte for byte rather than summarised.

#include "../Source/Protocol/RndProtocol.h"
#include "SmfScanner.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    int failures = 0;
    int checks   = 0;

    void report (bool ok, const char* expression, const char* file, int line,
                 const std::string& detail)
    {
        ++checks;
        if (ok)
            return;

        ++failures;
        std::printf ("FAIL %s:%d  %s\n", file, line, expression);
        if (! detail.empty())
            std::printf ("     %s\n", detail.c_str());
    }

    std::string hex (const std::vector<std::uint8_t>& bytes)
    {
        std::string out;
        char buffer[4];
        for (auto b : bytes)
        {
            std::snprintf (buffer, sizeof (buffer), "%02X ", b);
            out += buffer;
        }
        if (! out.empty())
            out.pop_back();
        return out;
    }
}

#define CHECK(expr)              report ((expr), #expr, __FILE__, __LINE__, {})
#define CHECK_MSG(expr, detail)  report ((expr), #expr, __FILE__, __LINE__, (detail))

// The four frames the device sent in the capture, in the order they arrived.
static const std::vector<std::uint8_t> capturedSeedFrame
    { 0xF0, 0x6F, 0x62, 0x78, 0x10, 0x67, 0x59, 0x10, 0x52, 0x0A, 0xF7 };
static const std::vector<std::uint8_t> capturedDumpBeginFrame
    { 0xF0, 0x6F, 0x62, 0x78, 0x20, 0xF7 };
static const std::vector<std::uint8_t> capturedGlobalsFrame
    { 0xF0, 0x6F, 0x62, 0x78, 0x21, 0x02, 0x7D, 0x00, 0x02, 0x11, 0xF7 };
static const std::vector<std::uint8_t> capturedEngineFrame
    { 0xF0, 0x6F, 0x62, 0x78, 0x22, 0x00, 0x00, 0x01, 0x46, 0x4D, 0x00, 0xF7 };

static constexpr std::uint32_t capturedSeed = 0xaa442ce7u;

static void testSeedPacking()
{
    // Round trip across the whole range, including the values that exercise the
    // truncated fifth byte.
    for (std::uint32_t seed : { 0x00000000u, 0x00000001u, 0x0000007Fu, 0x00000080u,
                                0x0FFFFFFFu, 0x10000000u, 0x7FFFFFFFu, 0x80000000u,
                                0xFFFFFFFFu, 0xDEADBEEFu, capturedSeed })
    {
        CHECK (rnd::unpackSeed (rnd::packSeed (seed)) == seed);
    }

    // Every packed byte must be a legal MIDI data byte.
    for (std::uint32_t seed : { 0xFFFFFFFFu, 0xAAAAAAAAu, 0x55555555u })
        for (auto b : rnd::packSeed (seed))
            CHECK ((b & 0x80) == 0);

    // The exact bytes the device sent.
    const rnd::SeedBytes expected { 0x67, 0x59, 0x10, 0x52, 0x0A };
    CHECK (rnd::packSeed (capturedSeed) == expected);
    CHECK (rnd::unpackSeed (expected) == capturedSeed);
}

static void testSeedTextRoundTrip()
{
    CHECK (rnd::formatSeed (capturedSeed) == "0xaa442ce7");
    CHECK (rnd::formatSeed (0) == "0x00000000");

    CHECK (rnd::parseSeed ("0xaa442ce7") == capturedSeed);
    CHECK (rnd::parseSeed ("0xAA442CE7") == capturedSeed);
    CHECK (rnd::parseSeed ("  0xaa442ce7  ") == capturedSeed);
    CHECK (rnd::parseSeed ("aa442ce7") == capturedSeed);

    // Digits-only input is decimal, so a tempo-looking number means itself.
    CHECK (rnd::parseSeed ("125") == 125u);
    CHECK (rnd::parseSeed ("0x125") == 0x125u);

    CHECK (rnd::parseSeed ("") == std::nullopt);
    CHECK (rnd::parseSeed ("   ") == std::nullopt);
    CHECK (rnd::parseSeed ("0xdeadbeeff") == std::nullopt);  // 9 hex digits
    CHECK (rnd::parseSeed ("4294967296") == std::nullopt);    // 2^32
    CHECK (rnd::parseSeed ("nope!") == std::nullopt);

    CHECK (rnd::parseSeed (rnd::formatSeed (0xFFFFFFFFu)) == 0xFFFFFFFFu);
}

static void testEncoding()
{
    const auto seedMessage = rnd::makeSeedMessage (capturedSeed);
    CHECK_MSG (seedMessage == capturedSeedFrame,
               "built " + hex (seedMessage) + ", device sent " + hex (capturedSeedFrame));

    const auto unlock = rnd::makeUnlockAndDump();
    const std::vector<std::uint8_t> expectedUnlock { 0xF0, 0x6F, 0x62, 0x78, 0x11, 0x00, 0xF7 };
    CHECK_MSG (unlock == expectedUnlock, "built " + hex (unlock));

    // Anything we build must parse back to the same thing.
    for (std::uint32_t seed : { 0u, 1u, capturedSeed, 0xFFFFFFFFu })
    {
        const auto parsed = rnd::parseSysex (rnd::makeSeedMessage (seed));
        CHECK (parsed.has_value());
        if (parsed)
        {
            const auto* message = std::get_if<rnd::SeedMessage> (&*parsed);
            CHECK (message != nullptr && message->seed == seed);
        }
    }
}

static void testParsingCapturedFrames()
{
    {
        const auto parsed = rnd::parseSysex (capturedSeedFrame);
        CHECK (parsed.has_value());
        const auto* message = parsed ? std::get_if<rnd::SeedMessage> (&*parsed) : nullptr;
        CHECK (message != nullptr);
        if (message)
            CHECK_MSG (message->seed == capturedSeed, rnd::formatSeed (message->seed));
    }

    {
        // 0x20 carries no payload and is not handled by the Seed Lab web app.
        const auto parsed = rnd::parseSysex (capturedDumpBeginFrame);
        CHECK (parsed.has_value());
        CHECK (parsed && std::get_if<rnd::DumpBeginMessage> (&*parsed) != nullptr);
    }

    {
        const auto parsed = rnd::parseSysex (capturedGlobalsFrame);
        CHECK (parsed.has_value());
        const auto* message = parsed ? std::get_if<rnd::GlobalsMessage> (&*parsed) : nullptr;
        CHECK (message != nullptr);
        if (message)
        {
            CHECK (message->patchMode == 2);
            CHECK (message->tempoBpm == 125);
            CHECK (message->tonic == 2);
            CHECK (std::string (rnd::tonicName (message->tonic)) == "D");
            CHECK (message->scaleIndex == 17);
            CHECK (std::string (rnd::scaleName (message->scaleIndex)) == "prometheus");
        }
    }

    {
        const auto parsed = rnd::parseSysex (capturedEngineFrame);
        CHECK (parsed.has_value());
        const auto* message = parsed ? std::get_if<rnd::TrackEngineMessage> (&*parsed) : nullptr;
        CHECK (message != nullptr);
        if (message)
        {
            CHECK (message->trackIndex == 0);
            CHECK (message->unknownA == 0x00);
            CHECK (message->unknownB == 0x01);
            CHECK_MSG (message->engineName == "FM", "got '" + message->engineName + "'");
        }
    }
}

static void testParsingRejectsJunk()
{
    CHECK (rnd::parseSysex (nullptr, 0) == std::nullopt);
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> {}) == std::nullopt);

    // Another manufacturer's frame.
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> { 0xF0, 0x43, 0x00, 0x01, 0x10, 0xF7 })
           == std::nullopt);

    // Right tag, unknown command.
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> { 0xF0, 0x6F, 0x62, 0x78, 0x7A, 0xF7 })
           == std::nullopt);

    // Right tag and command, payload too short.
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> { 0xF0, 0x6F, 0x62, 0x78, 0x10, 0x01, 0xF7 })
           == std::nullopt);
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> { 0xF0, 0x6F, 0x62, 0x78, 0x21, 0x02, 0x7D, 0xF7 })
           == std::nullopt);

    // dumpBegin must be empty — a payload means we misread the command.
    CHECK (rnd::parseSysex (std::vector<std::uint8_t> { 0xF0, 0x6F, 0x62, 0x78, 0x20, 0x01, 0xF7 })
           == std::nullopt);

    // Our own outbound unlock, echoed back by a loopback port, is not state.
    CHECK (rnd::parseSysex (rnd::makeUnlockAndDump()) == std::nullopt);

    // Hosts vary on whether they keep the F0/F7 wrapper. All four forms work.
    const std::vector<std::uint8_t> noF0 (capturedSeedFrame.begin() + 1, capturedSeedFrame.end());
    const std::vector<std::uint8_t> noF7 (capturedSeedFrame.begin(), capturedSeedFrame.end() - 1);
    const std::vector<std::uint8_t> bare (capturedSeedFrame.begin() + 1, capturedSeedFrame.end() - 1);
    CHECK (rnd::parseSysex (noF0).has_value());
    CHECK (rnd::parseSysex (noF7).has_value());
    CHECK (rnd::parseSysex (bare).has_value());
}

static void testForeignVersusDamaged()
{
    // Real frames Logic Pro delivered to the probe on an instrument track
    // (2026-08-10). Both are well-formed universal SysEx of Logic's own; the
    // point is that "not an RND frame" must never be read as "damaged", or the
    // probe reports a host that passes SysEx perfectly as one that mangles it.
    const std::vector<std::uint8_t> logicMasterTuning { 0x7F, 0x00, 0x04, 0x03, 0x00, 0x40 };

    std::vector<std::uint8_t> logicBulkTuning { 0x7E, 0x7F, 0x08, 0x01, 0x00 };
    for (char c : std::string ("Logic Tuning    "))
        logicBulkTuning.push_back (static_cast<std::uint8_t> (c));
    for (int note = 0; note < 128; ++note)
    {
        logicBulkTuning.push_back (static_cast<std::uint8_t> (note));
        logicBulkTuning.push_back (0x00);
        logicBulkTuning.push_back (0x00);
    }
    logicBulkTuning.push_back (0x37);

    const std::vector<const std::vector<std::uint8_t>*> logicFrames { &logicMasterTuning, &logicBulkTuning };

    for (const auto* frame : logicFrames)
    {
        CHECK (! rnd::hasManufacturerTag (*frame));
        CHECK (rnd::parseSysex (*frame) == std::nullopt);
        CHECK (! rnd::describeForeignSysex (frame->data(), frame->size()).empty());
    }

    CHECK (rnd::describeForeignSysex (logicMasterTuning.data(), logicMasterTuning.size())
           == "universal real-time SysEx");
    CHECK (rnd::describeForeignSysex (logicBulkTuning.data(), logicBulkTuning.size())
           == "universal non-real-time SysEx");

    // Our frames are recognised as ours whether or not they parse, and with or
    // without the F0/F7 wrapper.
    CHECK (rnd::hasManufacturerTag (capturedSeedFrame));
    CHECK (rnd::hasManufacturerTag (std::vector<std::uint8_t> (capturedSeedFrame.begin() + 1,
                                                               capturedSeedFrame.end() - 1)));
    CHECK (rnd::describeForeignSysex (capturedSeedFrame.data(), capturedSeedFrame.size()).empty());

    // Our tag, truncated payload: damaged, not foreign. This is the one the
    // probe must shout about.
    const std::vector<std::uint8_t> ourFrameTruncated { 0xF0, 0x6F, 0x62, 0x78, 0x10, 0x67, 0xF7 };
    CHECK (rnd::hasManufacturerTag (ourFrameTruncated));
    CHECK (rnd::parseSysex (ourFrameTruncated) == std::nullopt);

    // Another vendor's frame, and degenerate input.
    CHECK (! rnd::hasManufacturerTag (std::vector<std::uint8_t> { 0xF0, 0x43, 0x10, 0x4C, 0xF7 }));
    CHECK (rnd::describeForeignSysex (nullptr, 0) == "empty SysEx");
    CHECK (! rnd::hasManufacturerTag (nullptr, 0));
    CHECK (! rnd::hasManufacturerTag (std::vector<std::uint8_t> { 0xF0, 0x6F }));
}

static void testControlLayer()
{
    // The band midpoints follow floor(3.2 + 6.4 * index) across the table.
    for (int i = 0; i < rnd::numScales; ++i)
    {
        const auto expected = static_cast<std::uint8_t> (3.2 + 6.4 * i);
        CHECK_MSG (rnd::scaleCcValue (i) == expected,
                   "scale " + std::to_string (i) + " → " + std::to_string (rnd::scaleCcValue (i)));
    }

    // Sending a band's midpoint must select that band back.
    for (int i = 0; i < rnd::numScales; ++i)
        CHECK (rnd::scaleIndexForCc (rnd::scaleCcValue (i)) == i);

    CHECK (rnd::scaleIndexForCc (0) == 0);
    CHECK (rnd::scaleIndexForCc (127) == rnd::numScales - 1);

    CHECK (rnd::tonicNoteNumber (0) == 60);
    CHECK (rnd::tonicNoteNumber (2) == 62);
    CHECK (rnd::tonicNoteNumber (11) == 71);
    CHECK (rnd::tonicNoteNumber (12) == 60);
    CHECK (rnd::tonicNoteNumber (-1) == 71);

    CHECK (std::string (rnd::scaleName (-1)) == "?");
    CHECK (std::string (rnd::scaleName (rnd::numScales)) == "?");
    CHECK (std::string (rnd::tonicName (12)) == "?");
}

static void testStatusAccumulation()
{
    rnd::DeviceStatus status;
    CHECK (! status.hasSeed());

    for (const auto* frame : { &capturedSeedFrame, &capturedDumpBeginFrame,
                               &capturedGlobalsFrame, &capturedEngineFrame })
    {
        const auto parsed = rnd::parseSysex (*frame);
        CHECK (parsed.has_value());
        if (parsed)
            status.apply (*parsed);
    }

    CHECK (status.hasSeed());
    CHECK (status.seed == capturedSeed);
    CHECK (status.tempoBpm == 125);
    CHECK (status.tonic == 2);
    CHECK (status.scaleIndex == 17);
    CHECK (status.patchMode == 2);
    CHECK (status.engines.size() == 1);
    if (status.engines.size() == 1)
    {
        CHECK (status.engines[0].index == 0);
        CHECK (status.engines[0].name == "FM");
    }

    // Engines arriving out of order end up sorted.
    for (std::uint8_t track : { std::uint8_t (3), std::uint8_t (1) })
    {
        const std::vector<std::uint8_t> f { 0xF0, 0x6F, 0x62, 0x78, 0x22,
                                            track, 0x00, 0x01,
                                            std::uint8_t ('A' + track), 0x00, 0xF7 };
        if (const auto parsed = rnd::parseSysex (f))
            status.apply (*parsed);
    }
    CHECK (status.engines.size() == 3);
    CHECK (status.engines[0].index == 0 && status.engines[1].index == 1
           && status.engines[2].index == 3);

    // A re-send for a track already known replaces it rather than duplicating.
    const std::vector<std::uint8_t> replace { 0xF0, 0x6F, 0x62, 0x78, 0x22,
                                              0x01, 0x00, 0x01, 'Z', 0x00, 0xF7 };
    if (const auto parsed = rnd::parseSysex (replace))
        status.apply (*parsed);
    CHECK (status.engines.size() == 3);
    CHECK (status.engines[1].name == "Z");

    // A fresh seed invalidates the engine names but keeps the globals, which
    // the device has not contradicted yet.
    if (const auto parsed = rnd::parseSysex (rnd::makeSeedMessage (0x1234u)))
        status.apply (*parsed);
    CHECK (status.seed == 0x1234u);
    CHECK (status.engines.empty());
    CHECK (status.tempoBpm == 125);
}

// ── The capture ─────────────────────────────────────────────────────────────

static void testHardwareCapture()
{
    const std::string path = std::string (RND_FIXTURE_DIR) + "/CymaRNDfirmUp.mid";

    rndtest::Capture capture;
    try
    {
        capture = rndtest::readSmf (path);
    }
    catch (const std::exception& e)
    {
        CHECK_MSG (false, std::string ("reading fixture: ") + e.what());
        return;
    }

    CHECK (capture.ticksPerQuarter == 480);

    // Every SysEx frame in the capture must decode. If a firmware update ever
    // adds a command we do not know, this is where it shows up.
    CHECK_MSG (capture.sysex.size() == 4, std::to_string (capture.sysex.size()) + " frames");

    rnd::DeviceStatus status;
    int decoded = 0;
    for (const auto& event : capture.sysex)
    {
        const auto parsed = rnd::parseSysex (event.bytes);
        CHECK_MSG (parsed.has_value(), "undecodable frame: " + hex (event.bytes));
        if (parsed)
        {
            ++decoded;
            status.apply (*parsed);
        }
    }
    CHECK (decoded == static_cast<int> (capture.sysex.size()));

    // The whole dump folds into the state the hardware was actually in.
    CHECK (status.seed == capturedSeed);
    CHECK (status.tempoBpm == 125);
    CHECK (status.tonic == 2);
    CHECK (status.scaleIndex == 17);
    CHECK (status.engines.size() == 1 && status.engines[0].name == "FM");

    // The device emits its internal sequences as ordinary notes, one channel
    // per track. This is what makes sequence capture a host recording job
    // rather than a protocol problem, so it is worth pinning down.
    std::map<int, int> notesPerChannel;
    std::set<int> pitchClasses;
    std::set<int> velocities;
    for (const auto& note : capture.noteOns)
    {
        ++notesPerChannel[note.channel];
        pitchClasses.insert (note.note % 12);
        velocities.insert (note.velocity);
    }

    CHECK_MSG (capture.noteOns.size() == 78, std::to_string (capture.noteOns.size()) + " note-ons");
    CHECK (notesPerChannel.size() == 3);
    CHECK (notesPerChannel[1] == 24);
    CHECK (notesPerChannel[2] == 17);
    CHECK (notesPerChannel[3] == 37);

    // Velocity is expressive, not a constant — a recorder must preserve it.
    CHECK (velocities.size() > 8);

    // The dump arrives after the music has been playing for a while, which is
    // the shape of a user pressing "read" mid-performance.
    CHECK (! capture.noteOns.empty() && ! capture.sysex.empty());
    CHECK (capture.sysex.front().tick > capture.noteOns.front().tick);
}

int main()
{
    testSeedPacking();
    testSeedTextRoundTrip();
    testEncoding();
    testParsingCapturedFrames();
    testParsingRejectsJunk();
    testForeignVersusDamaged();
    testControlLayer();
    testStatusAccumulation();
    testHardwareCapture();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
