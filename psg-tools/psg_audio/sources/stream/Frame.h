#pragma once

#include <stdint.h>
#include "debug/DebugPayload.h"

class Register
{
	uint8_t m_index;

public:
	enum Index
	{
		// bank A
		A_Fine    = 0x00, // 00
		A_Coarse  = 0x01, // 01
		B_Fine    = 0x02, // 02
		B_Coarse  = 0x03, // 03
		C_Fine    = 0x04, // 04
		C_Coarse  = 0x05, // 05
		N_Period  = 0x06, // 06
		Mixer     = 0x07, // 07
		A_Volume  = 0x08, // 08
		B_Volume  = 0x09, // 09
		C_Volume  = 0x0A, // 10
		EA_Fine   = 0x0B, // 11
		EA_Coarse = 0x0C, // 12
		EA_Shape  = 0x0D, // 13

		// bank B
		EB_Fine   = 0x10, // 14
		EB_Coarse = 0x11, // 15
		EC_Fine   = 0x12, // 16
		EC_Coarse = 0x13, // 17
		EB_Shape  = 0x14, // 18
		EC_Shape  = 0x15, // 19
		A_Duty    = 0x16, // 20
		B_Duty    = 0x17, // 21
		C_Duty    = 0x18, // 22
		N_AndMask = 0x19, // 23
		N_OrMask  = 0x1A, // 24

		// aliases
		E_Fine    = EA_Fine,
		E_Coarse  = EA_Coarse,
		E_Shape   = EA_Shape,
		Mode_Bank = EA_Shape,

		A_Period  = A_Fine,
		B_Period  = B_Fine,
		C_Period  = C_Fine,
		E_Period  = E_Fine,
		EA_Period = EA_Fine,
		EB_Period = EB_Fine,
		EC_Period = EC_Fine,

		BankA_Fst = 0x00,
		BankA_Lst = 0x0D,
		BankB_Fst = 0x10,
		BankB_Lst = 0x1D,
	};

	Register(const uint8_t& index) : m_index(index) {}
	Register(const Index&   index) : m_index(static_cast<uint8_t>(index)) {}
	Register& operator= (const Index& index) { m_index = static_cast<uint8_t>(index); return *this; }
	operator uint8_t&() { return m_index; }

	static const Index t_fine   [];
	static const Index t_coarse [];
	static const Index t_duty   [];
	static const Index volume   [];
	static const Index e_fine   [];
	static const Index e_coarse [];
	static const Index e_shape  [];
};

class PRegister
{
	uint8_t m_index;

public:
	enum Index
	{
		A_Period  = Register::A_Fine,
		B_Period  = Register::B_Fine,
		C_Period  = Register::C_Fine,
		N_Period  = Register::N_Period,
		E_Period  = Register::E_Fine,
		EA_Period = Register::EA_Fine,
		EB_Period = Register::EB_Fine,
		EC_Period = Register::EC_Fine
	};

	PRegister(const Index& index) : m_index(static_cast<uint8_t>(index)) {}
	PRegister& operator= (const Index& index) { m_index = static_cast<uint8_t>(index); return *this; }
	operator Register () { return Register(m_index); }
	operator uint8_t& () { return m_index; }

	static const Index t_period[];
	static const Index e_period[];
};

using FrameId = uint32_t;
constexpr uint8_t c_modeBankRegIdx = 0x0D;
constexpr uint8_t c_unchangedShape = 0xFF;

class Frame : public DebugPayload
{
public:
	Frame();
	Frame(const Frame& other);
	
	void SetId(FrameId id);
	FrameId GetId() const;
	
	Frame& operator!  ();
	Frame& operator+= (const Frame& other);

	void ResetData();
	void ResetChanges();
	void SetChanges();
	bool HasChanges() const;
	bool IsAudible() const;

	uint32_t GetDataHash() const;
	uint32_t GetChangesHash() const;

	// debug payload
	void print_payload (std::ostream& stream) const override;
	void print_footer  (std::ostream& stream) const override;

private:
	void UpdateHashes() const;

public:
	struct Channel
	{
		enum { A = 0, B = 1, C = 2 };
	
		uint8_t tFine;
		uint8_t	tCoarse;
		uint8_t tDuty;   // doesn't matter in comp mode
		uint8_t	mixer;   // taken chan Tone & Noise bits
		uint8_t volume;
		uint8_t	eFine;   // taken from chan A in comp mode
		uint8_t	eCoarse; // taken from chan A in comp mode
		uint8_t	eShape;  // taken from chan A in comp mode + exp mode flags
	};

	class Registers
	{
		struct Info { uint8_t flags, index, mask; };
		bool GetInfo(Register reg, Info& info) const;

	public:
		void ResetData();
		void ResetChanges();
		void SetChanges();
		bool HasChanges() const;
		void ResetExpMode();
		void SetExpMode();
		bool IsExpMode() const;

	public:
		const uint8_t tmask(int chan) const;
		const uint8_t nmask(int chan) const;
		const uint8_t tmask() const; // all channels tone
		const uint8_t nmask() const; // all channels noise
		const uint8_t emask() const; // depends on exp mode
		const uint8_t vmask() const; // depends on exp mode

	public:
		uint8_t  Read(Register reg) const;
		uint16_t Read(PRegister preg) const;
		void     Read(int chan, Channel& data) const;

		bool IsChanged(Register reg) const;
		bool IsChanged(Register reg, uint8_t mask) const;
		bool IsChanged(PRegister preg) const;

		void Update(Register reg, uint8_t data);
		void Update(PRegister preg, uint16_t data);
		void Update(int chan, const Channel& data);

	public:
		uint8_t GetData(Register reg) const;
		uint8_t GetDiff(Register reg) const;

	public:
		bool IsToneEnabled(int chan) const;
		bool IsNoiseEnabled(int chan) const;
		bool IsEnvelopeEnabled(int chan) const;
		bool IsPeriodicEnvelope(int chan) const;
		uint8_t GetVolume(int chan) const;

	private:
		friend class Frame;
		uint8_t m_data[25];
		uint8_t m_diff[25];
		mutable bool m_update{ false };
	};

	const Registers& operator[](int chip) const;
	Registers& operator[](int chip);

private:
	FrameId m_id{ 0 };
	Registers m_regs[2];
	mutable uint32_t m_dhash{ 0 };
	mutable uint32_t m_chash{ 0 };
};
