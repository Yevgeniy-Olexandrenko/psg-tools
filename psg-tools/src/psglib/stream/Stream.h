#pragma once

#include <vector>
#include <filesystem>
#include "Property.h"
#include "Frame.h"
#include "Chip.h"

using FrameRate = uint16_t;

class Stream
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

		RW_PROP_DEF( std::string, title   );
		RW_PROP_DEF( std::string, artist  );
		RW_PROP_DEF( std::string, comment );
		RW_PROP_DEF( std::string, type    );

		RO_PROP_DEC( bool, titleKnown   );
		RO_PROP_DEC( bool, artistKnown  );
		RO_PROP_DEC( bool, commentKnown );
	};

	struct Play : public Delegate
	{
		Play(Stream& stream);
		friend class Stream;

		RO_PROP_DEC( size_t,    framesCount );
		RO_PROP_IMP( FrameId,   lastFrameId );
		RW_PROP_DEF( FrameRate, frameRate   );

	public:
		const Frame& GetFrame(FrameId frameId) const;

		void GetDuration(int& hh, int& mm, int& ss, int& ms) const;
		void GetDuration(int& hh, int& mm, int& ss) const;		

	private:
		void Prepare(FrameId loopFrameId, size_t loopFramesCount, FrameId lastFrameId);

	private:
		FrameId m_loopFrameId;
		size_t  m_loopFramesCount;
		size_t  m_extraLoops;
	};

public:
	enum class Property
	{
		Title, Artist, Comment, Type, Chip, Frames, Duration
	};

	Stream();
	std::string ToString(Property property) const;

	RO_PROP_DEC( size_t,  framesCount );
	RO_PROP_DEC( FrameId, lastFrameId );
	RO_PROP_DEF( FrameId, loopFrameId );
	RO_PROP_DEC( bool,    hasLoop     );
	
public:
	bool AddFrame(const Frame& frame);
	const Frame& GetFrame(FrameId frameId) const;
	void Finalize(FrameId loopFrameId);

	void GetDuration(int& hh, int& mm, int& ss, int& ms) const;
	void GetDuration(int& hh, int& mm, int& ss) const;

	bool IsSecondChipUsed() const;
	bool IsExpandedModeUsed() const;
	bool IsExpandedModeUsed(int chip) const;

	void ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss, int& ms) const;
	void ComputeDuration(size_t frameCount, int& hh, int& mm, int& ss) const;

public:
	File file;
	Info info;
	Play play;

	Chip schip; // source chip configuration defined by stream
	Chip dchip; // destination chip configuration defined by user

private:
	Frames m_frames;
	bool m_isSecondChipUsed;
	bool m_isExpandedModeUsed[2];
};
