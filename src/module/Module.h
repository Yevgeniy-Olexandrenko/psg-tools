#pragma once

#include <vector>
#include "Frame.h"

class Module
{
	friend std::ostream& operator<<(std::ostream& stream, const Module& module);

public:
	using FrameArray = std::vector<Frame>;
	using FrameIndex = uint32_t;
	using FrameRate  = uint16_t;

	Module();

public:
	// input/output file folder, name and ext
	void SetFilePath(const std::string& filePath);
	const std::string GetFilePath() const;
	const std::string GetFileName() const;
	const std::string GetFileExt() const;

	// song title (optional)
	void SetTitle(const std::string& title);
	const std::string& GetTitle() const;
	bool HasTitle() const;

	// song artist (optional)
	void SetArtist(const std::string& artist);
	const std::string& GetArtist() const;
	bool HasArtist() const;

	// input file type
	void SetType(const std::string& type);
	const std::string& GetType() const;

	// frame rate
	void SetFrameRate(FrameRate frameRate);
	FrameRate GetFrameRate() const;

	// add/get frames
	void AddFrame(const Frame& frame);
	const Frame& GetFrame(FrameIndex index) const;
	uint32_t GetFrameCount() const;

	// loop frame
	void SetLoopFrameUnavailable();
	void SetLoopFrameIndex(FrameIndex index);
	bool HasLoopFrameIndex() const;
	FrameIndex GetLoopFrameIndex() const;

private:
	std::string m_title;
	std::string m_artist;
	std::string m_type;

	struct {
		std::string m_folder;
		std::string m_name;
		std::string m_ext;
	} m_file;

	FrameRate  m_frameRate;
	FrameIndex m_loopFrameIndex;
	FrameArray m_frames;
};