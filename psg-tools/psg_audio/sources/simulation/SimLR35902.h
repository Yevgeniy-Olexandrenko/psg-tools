#pragma once

#include <map>
#include <array>
#include <vector>
#include "ChipSim.h"
#include "emulation/LR35902Apu.h"

// Simulates the APU of the Game Boy (DMG) on the PSG: the pulses go to the
// channels A and C, the wave to the channel B, the noise to a free channel.
class SimLR35902 : public ChipSim, public LR35902Apu
{
public:
	enum class OutputType { SingleChip, DoubleChip, AY8930Chip };

	SimLR35902();

	void ConfigureClock(int srcClock, int dstClock) override;
	void ConfigureOutput(OutputType outputType, Chip::Model model);

	void Reset() override;
	void Write(int chip, Register reg, uint8_t data) override;
	void Simulate(int samples) override;
	void Convert(Frame& frame) override;

private:
	struct State;
	using WaveRam = std::array<uint8_t, 16>;

	// The timbre of a waveform of the wave RAM, chosen once for the waveform: the
	// notes keep it whatever their pitch.
	enum class Timbre { Pulse, Envelope, Combo };
	struct WaveForm
	{
		int     multiple; // the period of the RAM holds this many periods of the pitch, 0 if flat
		float   rms;      // AC RMS of the samples, in steps of 4 bits
		Timbre  timbre;
		uint8_t duty;     // of the pulse: 0-4 are 3.125-50%
		int     envelope; // 0 saw, 1 triangle
		int     combo;    // index of the envelope gated by the tone
	};

	// The envelope gated by the tone of the same channel. One envelope cycle is the pitch
	// period and holds the tone periods in a whole number, so the two stay locked once
	// they start together; the tone starts high or low by chance: a profile for each.
	struct ComboModel
	{
		uint8_t shape;
		bool    triangle;     // a cycle of 64 steps, else 32
		int     periods;      // of the tone in the envelope cycle
		float   profiles[2][8];
	};

	void ConvertToSingleChip(State& state, Frame& frame);
	void ConvertToDoubleChip(State& state, Frame& frame);
	void ConvertToAY8930Chip(State& state, Frame& frame);

	float ChannelLevel(int chan, uint8_t volume) const;

	const WaveForm* AnalyzeWave();
	void BuildEnvelopeModels();
	void ConvertWave(const State& state, Frame::Registers& regs, uint8_t& mixer, uint8_t* volumes, bool expMode);

	uint8_t PulseVolume(const State& state, int index) const;
	void UpdateNoisePeriod(uint32_t shift);
	void PlaceNoise(State& state);
	void ConvertNoise(const State& state, Frame::Registers& regs, uint8_t& mixer, uint8_t* volumes, bool expMode) const;

private:
	OutputType m_outputType;
	uint32_t m_cyclesPerSample; // CPU clocks of an output sample, 16.16 fixed point
	uint32_t m_cpuCycles;       // the fraction left, 16.16 fixed point

	// the envelope of the destination chip as a saw and as a triangle:
	// the power shares of its harmonics and its AC RMS (full scale 1)
	struct EnvelopeModel { float profile[8]; float rms; } m_envelopes[2];
	std::vector<ComboModel> m_combos;
	std::map<WaveRam, WaveForm> m_waveForms; // analyses of the waveforms played
	bool    m_waveEnvelope;    // the wave played on the envelope in the last frame
	int     m_waveCombo;       // played in the last frame, -1 if none
	bool    m_waveSyncing;     // the tone was stopped in the last frame to start with the envelope
	bool    m_waveRestarted;   // by a trigger since the last frame
	uint16_t m_waveNote;       // 2048 - the period of the wave at the start of the note
	int     m_waveNoteAge;     // frames since the tone started with the envelope
	uint8_t m_waveShape;       // of the envelope in the last frame
	bool    m_wavePlayed;      // in the last frame
	int     m_noiseHead;       // frames left of the head of a hit in channel B

	uint8_t m_noiseChannels; // mask of the channels of the noise in the last frame
	int     m_noiseAge;      // frames since the last hit of the noise
	uint8_t m_noiseVolume;   // volume of the noise in the last frame
	bool    m_noiseRestarted; // by a trigger since the last frame

	// the PSG noise for the shift of the LFSR, computed when the shift changes
	uint32_t m_noiseShift;   // CPU clocks of a shift, 0 if not computed
	uint16_t m_noisePeriod;  // of the PSG noise, 1-31
	float    m_noiseHearing; // gain of the level that keeps the power heard
};
