#pragma once

#include <stdint.h>

class AY89XXPsg
{
	static constexpr int NUM_CHANNELS = 3;
	static constexpr int DECIMATE_FACTOR = 8;
	static constexpr int FIR_SIZE = 192;
	static constexpr int DC_FILTER_SIZE = 1024;

	class ToneUnit
	{
	public:
		void Reset();
		void Update(bool isExp);
		void SetPeriod(bool isExp, uint8_t fine, uint8_t coarse);
		void SetVolume(uint8_t volume);
		void SetDuty(uint8_t duty);
		int  GetOutput() const;
		int  GetVolume(bool isExp) const;
		int  GetEField(bool isExp, bool isWide) const;

	private:
		int m_counter;
		int m_period;
		int m_dutyCounter;
		int m_dutyCycle;
		int m_output;
		int m_volume;
	};

	class NoiseUnit
	{
	public:
		void Reset();
		void Update(bool isExp, bool isNew);
		void SetPeriod(bool isExp, uint8_t period);
		void SetMaskAND(uint8_t mask);
		void SetMaskOR(uint8_t mask);
		int  GetOutput() const;

	private:
		int m_prescale;
		int m_counter;
		int m_period;
		int m_value;
		int m_maskAND;
		int m_maskOR;
		int m_shift;
		int m_output;
	};

	class EnvelopeUnit
	{
	public:
		void Reset();
		void Update();
		void SetPeriod(uint8_t fine, uint8_t coarse);
		void SetShape(uint8_t shape);
		int  GetVolume() const;

	private:
		int m_counter;
		int m_period;
		int m_shape;
		int m_segment;
		int m_volume;
	};

	struct Interpolator
	{
		double c[4];
		double y[4];
	};

	struct DCFilter
	{
		double sum;
		double delay[DC_FILTER_SIZE];
	};

public:
	void Reset();
	void Write(uint8_t reg, uint8_t data);

	void Process();
	void RemoveDC();

	double GetOutA() const;
	double GetOutB() const;
	double GetOutC() const;

protected:
	enum class ChipType { AY8910, AY8914, AY8930, YM2149 };
	enum class PSGType  { AY, YM };

	AY89XXPsg(ChipType chipType, PSGType psgType, int clockRate, int sampleRate);

private:
	void WriteDirect(uint8_t reg, uint8_t data);
	void Process(double& outA, double& outB, double& outC);

	double Decimate(double* x) const;
	double FilterDC(DCFilter& filter, int index, double x) const;

	// inlines
	inline bool is_expanded_mode() { return ((m_chipType == ChipType::AY8930) && ((m_mode & 0xe) == 0xa)); }
	inline uint8_t get_register_bank() { return is_expanded_mode() ? (m_mode & 0x1) << 4 : 0; }

private: // emulation
	ChipType m_chipType;
	const double* m_dacTable;

	uint8_t m_regs[32];
	uint8_t m_mode;

	ToneUnit m_tone[NUM_CHANNELS];
	NoiseUnit m_noise;
	EnvelopeUnit m_envelope[NUM_CHANNELS];

private: // resampling
	double m_step;
	double m_counter;

	Interpolator m_interpolator[NUM_CHANNELS];

	double m_fir[NUM_CHANNELS][FIR_SIZE * 2];
	int m_firIndex;

	DCFilter m_dcFilter[NUM_CHANNELS];
	int m_dcFilterIndex;

	double m_out[NUM_CHANNELS];
};

class ChipAY8910 : public AY89XXPsg
{
public:
	ChipAY8910(int clock, int sampleRate)
		: AY89XXPsg(ChipType::AY8910, PSGType::AY, clock, sampleRate)
	{}
};

class ChipAY8914 : public AY89XXPsg
{
public:
	ChipAY8914(int clock, int sampleRate)
		: AY89XXPsg(ChipType::AY8914, PSGType::AY, clock, sampleRate)
	{}
};

class ChipAY8930 : public AY89XXPsg
{
public:
	ChipAY8930(int clock, int sampleRate)
		: AY89XXPsg(ChipType::AY8930,  PSGType::YM, clock, sampleRate)
	{}
};

class ChipYM2149 : public AY89XXPsg
{
public:
	ChipYM2149(int clock, int sampleRate)
		: AY89XXPsg(ChipType::YM2149, PSGType::YM, clock, sampleRate)
	{}
};
