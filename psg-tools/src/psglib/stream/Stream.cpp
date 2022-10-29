#include <iomanip>
#include <sstream>
#include "Stream.h"

namespace
{
	const int k_prefDurationMM = 4;
	const int k_prefDurationSS = 0;
	const int k_maxExtraLoops  = 3;
	const int k_maxSilenceSec  = 1;
}

////////////////////////////////////////////////////////////////////////////////

Stream::Info::Info(Stream& stream)
	: Delegate(stream)
{
}

bool Stream::Info::titleKnown() const
{
	return !m_title.empty();
}

bool Stream::Info::artistKnown() const
{
	return !m_artist.empty();
}

bool Stream::Info::commentKnown() const
{
	return !m_comment.empty();
}

////////////////////////////////////////////////////////////////////////////////

Stream::Play::Play(Stream& stream)
	: Delegate(stream)
	, m_frameRate(50)
	, m_loopFrameId(0)
	, m_loopFramesCount(0)
	, m_extraLoops(0)
{
}

size_t Stream::Play::framesCount() const
{
	if (m_lastFrameId == m_stream.lastFrameId())
	{
		return (m_stream.framesCount() + m_loopFramesCount * m_extraLoops);
	}
	return size_t(m_lastFrameId + 1);
}

FrameId Stream::Play::lastFrameId() const
{
	return FrameId(framesCount() - 1);
}

const Frame& Stream::Play::GetFrame(FrameId frameId) const
{
	if (frameId >= m_stream.framesCount())
	{
		frameId = m_loopFrameId + FrameId((frameId - m_stream.framesCount()) % m_loopFramesCount);
	}
	return m_stream.GetFrame(frameId);
}

void Stream::Play::GetDuration(int& hh, int& mm, int& ss, int& ms) const
{
	m_stream.ComputeDuration(framesCount(), hh, mm, ss, ms);
}

void Stream::Play::GetDuration(int& hh, int& mm, int& ss) const
{
	m_stream.ComputeDuration(framesCount(), hh, mm, ss);
}

void Stream::Play::Prepare(FrameId loopFrameId, size_t loopFramesCount, FrameId lastFrameId)
{
	m_loopFrameId = loopFrameId;
	m_loopFramesCount = loopFramesCount;
	m_lastFrameId = lastFrameId;

	if (m_loopFrameId < m_stream.framesCount() / 2)
	{
		// playback loop available, compute new duration using maximum extra loops amount
		auto maxPlaybackFrames = size_t((k_prefDurationMM * 60 + k_prefDurationSS) * frameRate());
		if (maxPlaybackFrames > m_stream.framesCount())
		{
			m_extraLoops = int((maxPlaybackFrames - m_stream.framesCount()) / m_loopFramesCount);
			m_extraLoops = std::min(m_extraLoops, size_t(k_maxExtraLoops));
		}
	}
	else
	{
		// playback loop not available, cut the silence at the end of the stream
		for (FrameId frameId = m_stream.lastFrameId(); frameId > 0; --frameId)
		{
			if (m_stream.GetFrame(frameId).IsAudible())
			{
				lastFrameId = (frameId + frameRate() * k_maxSilenceSec);
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
	, m_isSecondChipUsed(false)
	, m_isExpandedModeUsed{}
{
}

std::string Stream::ToString(Property property) const
{
	std::stringstream stream;
	switch (property)
	{
	case Property::Title:
		stream << info.title();
		break;

	case Property::Artist:
		stream << info.artist();
		break;

	case Property::Comment:
		stream << info.comment();
		break;

	case Property::Type:
		stream << info.type();
		break;;

	case Property::Chip:
		return schip.toString();

	case Property::Frames:
		stream << framesCount();
		if (hasLoop()) stream << " -> " << m_loopFrameId;
		stream << " @ " << play.frameRate() << " Hz";
		break;

	case Property::Duration:
		int hh0 = 0, mm0 = 0, ss0 = 0, ms0 = 0;
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
		break;
	}
	return stream.str();
}

size_t Stream::framesCount() const
{
	return m_frames.size();
}

FrameId Stream::lastFrameId() const
{
	return FrameId(framesCount() - 1);
}

bool Stream::hasLoop() const
{
	return (m_loopFrameId < framesCount() / 2);
}

bool Stream::AddFrame(const Frame& frame)
{
	if (framesCount() < 100000)
	{
		m_frames.push_back(frame);
		m_frames.back().SetId(lastFrameId());

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

void Stream::Finalize(FrameId loopFrameId)
{
	m_loopFrameId = (loopFrameId && loopFrameId < framesCount() ? loopFrameId : FrameId(framesCount()));
	loopFrameId = (m_loopFrameId > 2 ? m_loopFrameId : FrameId(framesCount()));
	play.Prepare(loopFrameId, framesCount() - loopFrameId, lastFrameId());
	ConfigureDestinationChip();
}

void Stream::GetDuration(int& hh, int& mm, int& ss, int& ms) const
{
	ComputeDuration(framesCount(), hh, mm, ss, ms);
}

void Stream::GetDuration(int& hh, int& mm, int& ss) const
{
	ComputeDuration(framesCount(), hh, mm, ss);
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
	auto duration = (frameCount * 1000) / play.frameRate();
	ms = int(duration % 1000); duration /= 1000;
	ss = int(duration % 60);   duration /= 60;
	mm = int(duration % 60);   duration /= 60;
	hh = int(duration);
}

void Stream::ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss) const
{
	auto duration = (((frameCount * 1000) / play.frameRate()) + 500) / 1000;
	ss = int(duration % 60); duration /= 60;
	mm = int(duration % 60); duration /= 60;
	hh = int(duration);
}

void Stream::ConfigureDestinationChip()
{
	// configure model of the 1st destination chip
	if (dchip.first.model() == Chip::Model::Compatible)
	{
		switch (schip.first.model())
		{
		case Chip::Model::AY8930:
		case Chip::Model::YM2149:
			dchip.first.model(schip.first.model());
			break;
		default:
			dchip.first.model(Chip::Model::AY8910);
			break;
		}
	}

	// configure model of the 2nd destination chip
	if (schip.second.modelKnown())
	{
		if (!dchip.second.modelKnown() || dchip.second.model() == Chip::Model::Compatible)
		{
			switch (schip.second.model())
			{
			case Chip::Model::AY8910:
			case Chip::Model::AY8930:
			case Chip::Model::YM2149:
				dchip.second.model(schip.second.model());
				break;
			default:
				dchip.second.model(dchip.first.model());
				break;
			}
		}
	}
	else
	{
		dchip.second.model(Chip::Model::Unknown);
	}

	// configure clock rate, output type and stereo type of destination chip
	if (!dchip.clockKnown ()) dchip.clock (schip.clockKnown () ? schip.clock () : Chip::Clock::F1750000);
	if (!dchip.outputKnown()) dchip.output(schip.outputKnown() ? schip.output() : Chip::Output::Stereo);
	if (!dchip.stereoKnown()) dchip.stereo(schip.stereoKnown() ? schip.stereo() : Chip::Stereo::ABC);

	// restrict stereo modes available for exp mode
	if (IsExpandedModeUsed() && dchip.output() == Chip::Output::Stereo)
	{
		if (dchip.stereo() != Chip::Stereo::ABC && dchip.stereo() != Chip::Stereo::ACB)
		{
			dchip.stereo(Chip::Stereo::ABC);
		}
	}
}
