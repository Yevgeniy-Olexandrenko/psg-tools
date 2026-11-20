#pragma once

#include <vector>
#include <filesystem>
#include "Frame.h"
#include "Chip.h"
#include "debug/DebugPayload.h"

// frames per second, it can be fractional; the formats that keep
// a whole number of frames per second take the rounded value
using FrameRate = double;
inline int RoundFrameRate(FrameRate frameRate) { return int(frameRate + 0.5); }

class Stream : public DebugPayload
{
    struct Delegate
    {
        Delegate(Stream& stream) : m_stream(stream) {}

    protected:
        Stream& m_stream;
    };

    using File = std::filesystem::path;
    using Frames = std::vector<Frame>;

public:
    struct Info : public Delegate
    {
        Info(Stream& stream);

        Property<std::string>  title;
        Property<std::string>  artist;
        Property<std::string>  comment;
        Property<std::string>  type;
        ReadOnlyProperty<bool> titleKnown;
        ReadOnlyProperty<bool> artistKnown;
        ReadOnlyProperty<bool> commentKnown;

    private:
        std::string m_title;
        std::string m_artist;
        std::string m_comment;
        std::string m_type;
    };

    struct Play : public Delegate
    {
        Play(Stream& stream);
        friend class Stream;

        Property<FrameRate>       frameRate;
        ReadOnlyProperty<size_t>  framesCount;
        ReadOnlyProperty<FrameId> lastFrameId;

    public:
        const Frame& GetFrame(FrameId frameId) const;

        void GetDuration(int& hh, int& mm, int& ss, int& ms) const;
        void GetDuration(int& hh, int& mm, int& ss) const;		

    private:
        size_t GetFramesCount() const;
        void Prepare(FrameId loopFrameId, size_t loopFramesCount, FrameId lastFrameId, bool loopFound);

    private:
        FrameRate m_frameRate;
        FrameId   m_lastFrameId;
        FrameId   m_loopFrameId;
        size_t    m_loopFramesCount;
        size_t    m_extraLoops;
    };

public:
    enum class Property
    {
        Tag, File, Title, Artist, Comment, Type, Chip, Frames, Duration
    };

    Stream();
    std::string ToString(Property property) const;

    ReadOnlyProperty<size_t>  framesCount;
    ReadOnlyProperty<FrameId> lastFrameId;
    ReadOnlyProperty<FrameId> loopFrameId;
    ReadOnlyProperty<bool>    hasLoop;
    
public:
    bool AddFrame(const Frame& frame);
    const Frame& GetFrame(FrameId frameId) const;
    // findLoop: the frames come from a register stream, which can
    // hold more than one pass of a looped song (see FindLoop).
    // length: the frames to keep when no loop is found (0 keeps all). A
    // decoder that plays the song past its length to find the loop gives it.
    // loopKeys: a key of each frame for the loop search, instead of the keys
    // of its registers (a machine gives the keys of the writes of its player).
    void Finalize(FrameId loopFrameId, bool findLoop = false, size_t length = 0,
        const std::vector<uint64_t>* loopKeys = nullptr);

    void GetDuration(int& hh, int& mm, int& ss, int& ms) const;
    void GetDuration(int& hh, int& mm, int& ss) const;

    bool IsSecondChipUsed() const;
    bool IsExpandedModeUsed() const;
    bool IsExpandedModeUsed(int chip) const;

    void ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss, int& ms) const;
    void ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss) const;

    // debug payload
    void print_header  (std::ostream& stream) const override;
    void print_payload (std::ostream& stream) const override;
    void print_footer  (std::ostream& stream) const override;

private:
    bool FindLoop(FrameId& loopFrameId, const std::vector<uint64_t>* loopKeys);
    void ConfigureDestinationChip();

public:
    File file;
    Info info;
    Play play;

    Chip schip; // source chip configuration defined by stream
    Chip dchip; // destination chip configuration defined by user

private:
    Frames  m_frames;
    FrameId m_loopFrameId;
    bool    m_loopFound; // the loop search found the loop, the file did not give it
    bool m_isSecondChipUsed;
    bool m_isExpandedModeUsed[2];
};
