#include <algorithm>
#include <iomanip>
#include <sstream>
#include "Stream.h"

namespace
{
    const int k_prefDurationMM = 4;
    const int k_prefDurationSS = 0;
    const int k_maxExtraLoops  = 3;
    const int k_maxSilenceSec  = 1;
    const int k_minLoopSec     = 4;

    // A loop at this frame or before is no loop: the whole song repeats, and
    // it plays once. A player or a logger can start such a loop after a few
    // frames. A loop has a use only after an intro: the player repeats it to
    // make a short song longer. Real intros are 0.1 s at least (6 frames).
    const FrameId k_noLoopFrame = 5;

    // The key of a frame for the loop search: the registers that make the sound.
    // A shape register keeps the last shape written. In the first pass, it can
    // come from the intro; in the next passes, from the end of the song. Thus
    // the shape counts only when a channel plays the envelope or the shape is
    // written (a write restarts the envelope).
    uint64_t GetLoopKey(const Frame& frame)
    {
        uint64_t key = 0xCBF29CE484222325; // FNV-1a
        const auto Mix = [&](uint8_t data) { key = (key ^ data) * 0x100000001B3; };

        for (int chip = 0; chip < 2; ++chip)
        {
            const Frame::Registers& regs = frame[chip];
            for (Register reg = Register::BankA_Fst; reg <= Register::BankB_Lst; ++reg)
            {
                if (reg == Register::EA_Shape || reg == Register::EB_Shape || reg == Register::EC_Shape) continue;
                Mix(regs.GetData(reg));
            }

            // the mode bits are in the register of the shape of envelope A
            Mix(regs.GetData(Register::Mode_Bank) & 0xF0);

            for (int chan = 0; chan < 3; ++chan)
            {
                Register shape = (regs.IsExpMode() ? Register::e_shape[chan] : Register::E_Shape);
                bool written = regs.IsChanged(shape);
                Mix(written);
                Mix(written || regs.IsEnvelopeEnabled(chan) ? regs.GetData(shape) & 0x0F : 0xFF);
            }
        }
        return key;
    }
}

////////////////////////////////////////////////////////////////////////////////

Stream::Info::Info(Stream& stream)
    : Delegate(stream)
    , title(m_title)
    , artist(m_artist)
    , comment(m_comment)
    , type(m_type)
    , titleKnown([this]() { return !m_title.empty(); })
    , artistKnown([this]() { return !m_artist.empty(); })
    , commentKnown([this]() { return !m_comment.empty(); })
{
}

////////////////////////////////////////////////////////////////////////////////

Stream::Play::Play(Stream& stream)
    : Delegate(stream)
    , m_frameRate(50)
    , m_loopFrameId(0)
    , m_loopFramesCount(0)
    , m_extraLoops(0)
    , frameRate(m_frameRate)
    , framesCount([this]() { return this->GetFramesCount(); })
    , lastFrameId([this]() { return FrameId(framesCount - 1); })
{
}

const Frame& Stream::Play::GetFrame(FrameId frameId) const
{
    if (frameId >= m_stream.framesCount)
    {
        frameId = m_loopFrameId + FrameId((frameId - m_stream.framesCount) % m_loopFramesCount);
    }
    return m_stream.GetFrame(frameId);
}

void Stream::Play::GetDuration(int& hh, int& mm, int& ss, int& ms) const
{
    m_stream.ComputeDuration(framesCount, hh, mm, ss, ms);
}

void Stream::Play::GetDuration(int& hh, int& mm, int& ss) const
{
    m_stream.ComputeDuration(framesCount, hh, mm, ss);
}

size_t Stream::Play::GetFramesCount() const
{
    if (m_lastFrameId == m_stream.lastFrameId)
    {
        return (m_stream.framesCount + m_loopFramesCount * m_extraLoops);
    }
    return size_t(m_lastFrameId + 1);
}

void Stream::Play::Prepare(FrameId loopFrameId, size_t loopFramesCount, FrameId lastFrameId, bool loopFound)
{
    m_loopFrameId = loopFrameId;
    m_loopFramesCount = loopFramesCount;
    m_lastFrameId = lastFrameId;

    // an empty stream has no frames to loop or to cut
    if (!m_stream.framesCount) return;

    // A loop from the file plays again only when its body is longer than the
    // intro. A loop found in the frames repeats to the end of the recording,
    // so a shorter body is a loop too.
    if (m_loopFrameId < m_stream.framesCount / 2 || loopFound)
    {
        // playback loop available, compute new duration using maximum extra loops amount
        auto maxPlaybackFrames = size_t((k_prefDurationMM * 60 + k_prefDurationSS) * frameRate);
        if (maxPlaybackFrames > m_stream.framesCount)
        {
            m_extraLoops = int((maxPlaybackFrames - m_stream.framesCount) / m_loopFramesCount);
            m_extraLoops = std::min(m_extraLoops, size_t(k_maxExtraLoops));
        }
    }
    else
    {
        // playback loop not available, cut the silence at the end of the stream
        for (FrameId frameId = m_stream.lastFrameId; frameId > 0; --frameId)
        {
            if (m_stream.GetFrame(frameId).IsAudible())
            {
                lastFrameId = FrameId(frameId + frameRate * k_maxSilenceSec);
                if (lastFrameId < m_lastFrameId) m_lastFrameId = lastFrameId;
                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

Stream::Stream()
    : info(*this)
    , play(*this)
    , m_loopFound(false)
    , m_isSecondChipUsed(false)
    , m_isExpandedModeUsed{}
    , framesCount([this]() { return m_frames.size(); })
    , lastFrameId([this]() { return FrameId(framesCount - 1); })
    , loopFrameId(m_loopFrameId)
    , hasLoop([this]() { return (m_loopFrameId > 0); })
{
}

std::string Stream::ToString(Property property) const
{
    std::stringstream stream;
    switch (property)
    {
    case Property::Tag:
    {	auto cleanup = [](const std::string& src) -> std::string
        {
            std::string dst{ src };
            std::transform(src.begin(), src.end(), dst.begin(),
                [](unsigned char c) 
                { 
                    if (c == ' ' || c == '.') c = '_';
                    if (c < 0x20 || c > 0xFF) c = '@';
                    return std::tolower(c);
                }
            );
            return dst;
        };
        stream << cleanup(file.filename().string());
    }	break;

    case Property::File:
        stream << file.string();
        break;

    case Property::Title:
        stream << (std::string)info.title;
        break;

    case Property::Artist:
        stream << (std::string)info.artist;
        break;

    case Property::Comment:
        stream << (std::string)info.comment;
        break;

    case Property::Type:
        stream << (std::string)info.type;
        break;;

    case Property::Chip:
        return schip.toString();

    case Property::Frames:
    {	stream << framesCount;
        if (hasLoop) stream << " -> " << loopFrameId;
        stream << " @ ";
        if (play.frameRate == RoundFrameRate(play.frameRate))
            stream << RoundFrameRate(play.frameRate);
        else
            stream << std::fixed << std::setprecision(2) << play.frameRate;
        stream << " Hz";
    }	break;

    case Property::Duration:
    {	int hh0 = 0, mm0 = 0, ss0 = 0, ms0 = 0;
        int hh1 = 0, mm1 = 0, ss1 = 0, ms1 = 0;
        GetDuration(hh0, mm0, ss0);
        play.GetDuration(hh1, mm1, ss1);

        stream <<
            std::setfill('0') << std::setw(2) << hh0 << ':' <<
            std::setfill('0') << std::setw(2) << mm0 << ':' <<
            std::setfill('0') << std::setw(2) << ss0;

        if (hh0 != hh1 || mm0 != mm1 || ss0 != ss1)
        {
            stream << " (";
            stream <<
                std::setfill('0') << std::setw(2) << hh1 << ':' <<
                std::setfill('0') << std::setw(2) << mm1 << ':' <<
                std::setfill('0') << std::setw(2) << ss1;
            stream << ')';
        }
    }	break;
    }
    return stream.str();
}

bool Stream::AddFrame(const Frame& frame)
{
    if (framesCount < 100000)
    {
        m_frames.push_back(frame);
        m_frames.back().SetId(lastFrameId);

        m_isSecondChipUsed |= m_frames.back()[1].HasChanges();
        m_isExpandedModeUsed[0] |= m_frames.back()[0].IsExpMode();
        m_isExpandedModeUsed[1] |= m_frames.back()[1].IsExpMode();
        return true;
    }
    return false;
}

const Frame& Stream::GetFrame(FrameId frameId) const
{
    return m_frames[frameId];
}

void Stream::Finalize(FrameId loopFrameId, bool findLoop, size_t length, const std::vector<uint64_t>* loopKeys)
{
    // a loop from the file stays a loop from the file when FindLoop keeps it
    const bool fileLoop = (loopFrameId > k_noLoopFrame && loopFrameId < m_frames.size());
    const bool found = (findLoop && FindLoop(loopFrameId, loopKeys));
    if (findLoop && !found && length && length < m_frames.size())
    {
        m_frames.resize(length);
    }
    m_loopFrameId = (loopFrameId > k_noLoopFrame && loopFrameId < framesCount ? loopFrameId : 0);
    m_loopFound = (found && !fileLoop && m_loopFrameId > 0);
    loopFrameId = (m_loopFrameId ? m_loopFrameId : FrameId(framesCount));

    play.Prepare(loopFrameId, framesCount - loopFrameId, lastFrameId, m_loopFound);
    ConfigureDestinationChip();
}

// A recording of more than one pass of a looped song ends with repeats: it
// loops from frame L with period P when frame[i] == frame[i + P] for i >= L.
// The Z-function of the reversed stream gives, for each P, the number of
// frames at the end that are equal to the frames P before them. Of the periods
// with one full repeat or more, the true loop starts first. A pattern played
// twice near the end starts later; multiples of the period start at the same
// frame. The frames after the first repeat are cut.
//
// A machine can give its own keys: the writes of its player, which repeat
// while the state of a chip that runs on its own can drift from pass to pass.
bool Stream::FindLoop(FrameId& loopFrameId, const std::vector<uint64_t>* loopKeys)
{
    const size_t count = m_frames.size();
    const size_t minPeriod = size_t(play.frameRate * k_minLoopSec);
    if (!minPeriod || count < 2 * minPeriod) return false;
    if (loopKeys && loopKeys->size() != count) loopKeys = nullptr;

    // the keys of the frames in the reverse order
    std::vector<uint64_t> keys(count);
    for (size_t i = 0; i < count; ++i)
    {
        keys[i] = (loopKeys ? (*loopKeys)[count - 1 - i] : GetLoopKey(m_frames[count - 1 - i]));
    }

    std::vector<uint32_t> z(count, 0);
    for (size_t i = 1, l = 0, r = 0; i < count; ++i)
    {
        size_t len = (i < r ? std::min(r - i, size_t(z[i - l])) : 0);
        while (i + len < count && keys[len] == keys[i + len]) ++len;
        if (i + len > r) { l = i; r = i + len; }
        z[i] = uint32_t(len);
    }

    // Running counts of the audible frames and of the frames that are not
    // equal to the frame before. They find a silent or frozen loop in O(1).
    std::vector<uint32_t> audible(count + 1, 0), changed(count + 1, 0);
    for (size_t i = 0; i < count; ++i)
    {
        size_t r = (count - 1 - i);
        audible[i + 1] = audible[i] + m_frames[i].IsAudible();
        changed[i + 1] = changed[i] + (i && keys[r] != keys[r + 1]);
    }

    size_t bestLoop = count, bestPeriod = 0;
    for (size_t period = minPeriod; period <= count / 2; ++period)
    {
        if (z[period] < period) continue;
        size_t loop = (count - period - z[period]);
        size_t end = (loop + period);

        if (loop < bestLoop
            && audible[end] > audible[loop]
            && changed[end] > changed[loop + 1])
        {
            bestLoop = loop;
            bestPeriod = period;
        }
    }

    // A pattern played some times at the end of a recorded song is not a
    // loop: it is shorter than the part before it. A machine plays its song
    // for minutes, and a repeat to the end of that is the loop: there, a loop
    // shorter than the intro is accepted when it repeats two times at least.
    if (!bestPeriod) return false;
    if (bestPeriod < bestLoop && !(loopKeys && count - bestLoop >= 3 * bestPeriod)) return false;

    if (loopFrameId > k_noLoopFrame && loopFrameId < count)
    {
        // The file has a loop: keep it, and cut only full passes after it.
        // Thus the repeats must start at the loop or before it, and the frames
        // from the loop to the end must be full periods. One pass has one
        // period from the loop to the end, and nothing is cut.
        if (bestLoop > loopFrameId || (count - loopFrameId) % bestPeriod) return false;
        bestLoop = loopFrameId;
    }

    m_frames.resize(bestLoop + bestPeriod);
    loopFrameId = FrameId(bestLoop);
    return true;
}

void Stream::GetDuration(int& hh, int& mm, int& ss, int& ms) const
{
    ComputeDuration(framesCount, hh, mm, ss, ms);
}

void Stream::GetDuration(int& hh, int& mm, int& ss) const
{
    ComputeDuration(framesCount, hh, mm, ss);
}

bool Stream::IsSecondChipUsed() const
{
    return m_isSecondChipUsed;
}

bool Stream::IsExpandedModeUsed() const
{
    return (IsExpandedModeUsed(0) || IsExpandedModeUsed(1));
}

bool Stream::IsExpandedModeUsed(int chip) const
{
    return m_isExpandedModeUsed[bool(chip)];
}

void Stream::ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss, int& ms) const
{
    auto duration = uint64_t(frameCount * 1000 / play.frameRate);
    ms = int(duration % 1000); duration /= 1000;
    ss = int(duration % 60);   duration /= 60;
    mm = int(duration % 60);   duration /= 60;
    hh = int(duration);
}

void Stream::ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss) const
{
    auto duration = uint64_t(frameCount / play.frameRate + 0.5);
    ss = int(duration % 60); duration /= 60;
    mm = int(duration % 60); duration /= 60;
    hh = int(duration);
}

void Stream::print_header(std::ostream& stream) const
{
    if (stream)
    {
        stream << "tag      : " << ToString(Property::Tag) << std::endl;
        stream << "file     : " << ToString(Property::File) << std::endl;
        stream << "title    : " << ToString(Property::Title) << std::endl;
        stream << "artist   : " << ToString(Property::Artist) << std::endl;
        stream << "comment  : " << ToString(Property::Comment) << std::endl;
        stream << "type     : " << ToString(Property::Type) << std::endl;
        stream << "chip     : " << ToString(Property::Chip) << std::endl;
        stream << "frames   : " << ToString(Property::Frames) << std::endl;
        stream << "duration : " << ToString(Property::Duration) << std::endl;
    }
}

void Stream::print_payload(std::ostream& stream) const
{
    if (stream)
    {
        int loop = (hasLoop ? loopFrameId : -1);
        for (FrameId frameId = 0; frameId < framesCount; ++frameId)
        {
            const Frame& frame = GetFrame(frameId);
            if (int(frameId) == loop)
                stream << ">LOOP : ";
            else
                stream << std::dec << std::setw(5) << std::setfill('0') << frameId << " : ";
            frame.print_payload(stream);
            frame.print_footer(stream);
            stream << std::endl;

            if ((frameId + 1) % RoundFrameRate(play.frameRate) == 0) stream << std::endl;
        }
    }
}

void Stream::print_footer(std::ostream& stream) const
{
    if (stream)
    {
        stream << "is_second_chip_used   : " << (IsSecondChipUsed() ? "yes" : "no") << std::endl;
        stream << "is_expanded_mode_used : " << (IsExpandedModeUsed() ? "yes" : "no");
    }
}

void Stream::ConfigureDestinationChip()
{
    // configure model of the 1st destination chip
    if (dchip.first.model == Chip::Model::Compatible)
    {
        switch (schip.first.model)
        {
        case Chip::Model::AY8930:
        case Chip::Model::YM2149:
            dchip.first.model = schip.first.model;
            break;
        default:
            dchip.first.model = Chip::Model::AY8910;
            break;
        }
    }

    // configure model of the 2nd destination chip
    if (schip.second.modelKnown && IsSecondChipUsed())
    {
        if (!dchip.second.modelKnown || dchip.second.model == Chip::Model::Compatible)
        {
            switch (schip.second.model)
            {
            case Chip::Model::AY8910:
            case Chip::Model::AY8930:
            case Chip::Model::YM2149:
                dchip.second.model = schip.second.model;
                break;
            default:
                dchip.second.model = dchip.first.model;
                break;
            }
        }
    }
    else
    {
        dchip.second.model = Chip::Model::Unknown;
    }

    // configure clock rate, output type and stereo type of destination chip
    if (!dchip.clockKnown ) dchip.clock  = (schip.clockKnown  ? schip.clock  : Chip::Clock::F1773400);
    if (!dchip.outputKnown) dchip.output = (schip.outputKnown ? schip.output : Chip::Output::Stereo);
    if (!dchip.stereoKnown) dchip.stereo = (schip.stereoKnown ? schip.stereo : Chip::Stereo::ABC);

    // restrict stereo modes available for exp mode
    if (IsExpandedModeUsed() && dchip.output == Chip::Output::Stereo)
    {
        if (dchip.stereo != Chip::Stereo::ABC && dchip.stereo != Chip::Stereo::ACB)
        {
            dchip.stereo = Chip::Stereo::ABC;
        }
    }
}
