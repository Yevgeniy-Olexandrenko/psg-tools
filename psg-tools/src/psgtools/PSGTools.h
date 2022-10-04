#pragma once

#include "PSGLib.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class PSGPlayer : public PSGHandler
{
    enum class PlayStreamResult { GoToNext, GoToPrevious, Termination };

public:
    PSGPlayer(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination);
    void Play();

protected:
	void OnFrameDecoded(Stream& stream, FrameId frameId) override;

private:
    PlayStreamResult PlayStream(const Stream& stream);

private:
    Chip& m_chip;
    Output& m_output;
    Filelist& m_filelist;
    Filelist& m_favorites;
    Termination& m_termination;

    Player m_player;
    Output::Enables m_enables;

    bool m_printStatic;
    size_t m_staticHeight;
    size_t m_dynamicHeight;
};

class PSGConverter : public PSGHandler
{
public:
    PSGConverter(Chip& chip, Filelist& filelist, Termination& termination);
    void Convert();

protected:
	void OnFrameDecoded(Stream& stream, FrameId frameId) override;
	void OnFrameEncoded(Stream& stream, FrameId frameId) override;

private:
    Chip& m_chip;
    Filelist& m_filelist;
    Termination& m_termination;
};