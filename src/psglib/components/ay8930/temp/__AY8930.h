/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2015  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2016 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/


#pragma once
#include "Channel.h"

////////////////////////////////////////////////////////////////////////////////

enum chan_id_t {
	CHANID_AY8930_CH1,
	CHANID_AY8930_CH2,
	CHANID_AY8930_CH3
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class CAY8930Channel : public CChannel
{
public:
	friend class CAY8930;

	CAY8930Channel(CMixer *pMixer, uint8_t ID);
	
	void Process(uint32_t Time);
	void RunEnvelope(uint32_t Time);
	void Reset();

	uint32_t GetTime();
	void Output(uint32_t Noise);

	double GetFrequency() const;

private:
	uint8_t m_iVolume;
	uint32_t m_iPeriod;
	uint32_t m_iPeriodClock;

	uint8_t m_iDutyCycleCounter;
	uint8_t m_iDutyCycle;

	bool m_bSquareHigh;
	bool m_bSquareDisable;
	bool m_bNoiseDisable;

	uint32_t m_iEnvelopePeriod;
	uint32_t m_iEnvelopeClock;
	char m_iEnvelopeLevel;
	char m_iEnvelopeShape;
	bool m_bEnvelopeHold;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class CAY8930
{
public:
	CAY8930(CMixer *pMixer);
	virtual ~CAY8930();
	
	void	Reset();
	void	Process(uint32_t Time);
	void	EndFrame();
	void	WriteReg(uint8_t Port, uint8_t Value);

private:
	void	RunNoise(uint32_t Time);

private:
	CMixer *m_pMixer;

	CAY8930Channel *m_pChannel[3];

	uint32_t m_iNoisePeriod;
	uint32_t m_iNoiseClock;
	uint32_t m_iNoiseState;
	uint32_t m_iNoiseValue;
	uint32_t m_iNoiseLatch;
	uint32_t m_iNoiseANDMask;
	uint32_t m_iNoiseORMask;

};
