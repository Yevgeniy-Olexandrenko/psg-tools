#include "Frame.h"
#include <cstring>
#include <iomanip>
#include <vector>

const Register::Index Register::t_fine   [] { A_Fine,    B_Fine,    C_Fine    };
const Register::Index Register::t_coarse [] { A_Coarse,  B_Coarse,  C_Coarse  };
const Register::Index Register::t_duty   [] { A_Duty,    B_Duty,    C_Duty    };
const Register::Index Register::volume   [] { A_Volume,  B_Volume,  C_Volume  };
const Register::Index Register::e_fine   [] { EA_Fine,   EB_Fine,   EC_Fine   };
const Register::Index Register::e_coarse [] { EA_Coarse, EB_Coarse, EC_Coarse };
const Register::Index Register::e_shape  [] { EA_Shape,  EB_Shape,  EC_Shape  };

const PRegister::Index PRegister::t_period [] { A_Period,  B_Period,  C_Period  };
const PRegister::Index PRegister::e_period [] { EA_Period, EB_Period, EC_Period };

namespace
{
	struct RegDefine
	{
		uint8_t comIndex; // 0xFF -> unknown register, b7 = 1 -> envelope shape register
		uint8_t comMask;  // mask for register in compatibility mode
		uint8_t expIndex; // 0xFF -> unknown register, b7 = 1 -> envelope shape register
		uint8_t expMask;  // mask for register in expanded mode
	};

	const RegDefine c_regDefines[] =
	{
		// bank A
		{ 0x00, 0xFF, 0x00, 0xFF },
		{ 0x01, 0x0F, 0x01, 0xFF },
		{ 0x02, 0xFF, 0x02, 0xFF },
		{ 0x03, 0x0F, 0x03, 0xFF },
		{ 0x04, 0xFF, 0x04, 0xFF },
		{ 0x05, 0x0F, 0x05, 0xFF },
		{ 0x06, 0x1F, 0x06, 0xFF },
		{ 0x07, 0x3F, 0x07, 0x3F },
		{ 0x08, 0x1F, 0x08, 0x3F },
		{ 0x09, 0x1F, 0x09, 0x3F },
		{ 0x0A, 0x1F, 0x0A, 0x3F },
		{ 0x0B, 0xFF, 0x0B, 0xFF },
		{ 0x0C, 0xFF, 0x0C, 0xFF },
		{ 0x8D, 0xEF, 0x8D, 0xEF },
		{ 0xFF, 0x00, 0xFF, 0x00 },
		{ 0xFF, 0x00, 0xFF, 0x00 },

		// bank B
		{ 0xFF, 0x00, 0x0E, 0xFF },
		{ 0xFF, 0x00, 0x0F, 0xFF },
		{ 0xFF, 0x00, 0x10, 0xFF },
		{ 0xFF, 0x00, 0x11, 0xFF },
		{ 0xFF, 0x00, 0x92, 0x0F },
		{ 0xFF, 0x00, 0x93, 0x0F },
		{ 0xFF, 0x00, 0x14, 0x0F },
		{ 0xFF, 0x00, 0x15, 0x0F },
		{ 0xFF, 0x00, 0x16, 0x0F },
		{ 0xFF, 0x00, 0x17, 0xFF },
		{ 0xFF, 0x00, 0x18, 0xFF },
		{ 0xFF, 0x00, 0xFF, 0x00 },
		{ 0xFF, 0x00, 0xFF, 0x00 },
		{ 0xFF, 0x00, 0x8D, 0xEF },
		{ 0xFF, 0x00, 0xFF, 0x00 },
		{ 0xFF, 0x00, 0xFF, 0x00 }
	};

	uint32_t UpdateHash(const uint8_t* data, size_t size, uint32_t hash = 0x00001505)
	{
		for (size_t i = 0; data && i < size; ++i)
		{
			hash = ((hash << 5) + hash + data[i]);
		}
		return hash;
	}
}

Frame::Frame()
{
	ResetData();
	ResetChanges();
}

Frame::Frame(const Frame& other)
	: m_id(other.m_id)
	, m_dhash(other.m_dhash)
	, m_chash(other.m_chash)
{
	for (int chip = 0; chip < 2; ++chip)
	{
		m_regs[chip] = other.m_regs[chip];
	}
}

void Frame::SetId(FrameId id)
{
	m_id = id;
}

FrameId Frame::GetId() const
{
	return m_id;
}

Frame& Frame::operator!()
{
	SetChanges();
	return *this;
}

Frame& Frame::operator+=(const Frame& other)
{
	for (int chip = 0; chip < 2; ++chip)
	{
		auto& dst = m_regs[chip];
		auto& src = other.m_regs[chip];

		// case when the chip mode is switching
		if (dst.IsExpMode() != src.IsExpMode())
		{
			memcpy(dst.m_data, src.m_data, 25);
			dst.SetChanges();
		}

		// case when the chip data is simply updating
		else for (Register reg = Register::BankA_Fst; reg <= Register::BankB_Lst; ++reg)
		{
			dst.Update(reg, src.Read(reg));
		}
	}
	return *this;
}

void Frame::ResetData()
{
	for (Registers& regs : m_regs) regs.ResetData();
}

void Frame::ResetChanges()
{
	for (Registers& regs : m_regs) regs.ResetChanges();
}

void Frame::SetChanges()
{
	for (Registers& regs : m_regs) regs.SetChanges();
}

bool Frame::HasChanges() const
{
	for (const Registers& regs : m_regs)
	{
		if (regs.HasChanges()) return true;
	}
	return false;
}

bool Frame::IsAudible() const
{
	for (const Registers& regs : m_regs)
	{
		for (int chan = 0; chan < 3; ++chan)
		{
			uint8_t mixer = regs.GetData(Register::Mixer);
			uint8_t volEn = regs.GetData(Register::volume[chan]);

			bool enableT = ~(mixer & regs.tmask(chan));
			bool enableN = ~(mixer & regs.nmask(chan));
			bool enableE =  (volEn & regs.emask());
			bool enableV =  (volEn & regs.vmask());

			if (((enableT || enableN) && enableV) || enableE) return true;
		}
	}
	return false;
}

uint32_t Frame::GetDataHash() const
{
	UpdateHashes();
	return m_dhash;
}

uint32_t Frame::GetChangesHash() const
{
	UpdateHashes();
	return m_chash;
}

void Frame::print_payload(std::ostream& stream) const
{
	if (stream)
	{
		for (const Registers& regs : m_regs)
		{
			Registers::Info info;
			auto count = (regs.IsExpMode() ? 32 : 16);
			for (Register reg = 0; reg < count; ++reg)
			{
				if (regs.GetInfo(reg, info))
				{
					int data = regs.m_data[info.index];
					int diff = regs.m_diff[info.index];

					if (diff)
						stream << '{' << std::hex << std::setw(2) << std::setfill('0') << data << '}';
					else
						stream << ' ' << std::hex << std::setw(2) << std::setfill('0') << data << ' ';
				}
				else stream << " -- ";
			}
			stream << ':';
		}
	}
}

void Frame::print_footer(std::ostream& stream) const
{
	stream << ' ' << std::hex << std::setw(8) << std::setfill('0') << GetDataHash();
	stream << ' ' << std::hex << std::setw(8) << std::setfill('0') << GetChangesHash();
	if (!HasChanges()) stream << " no_changes";
	if (!IsAudible()) stream << " not_audible";
}

void Frame::UpdateHashes() const
{
	if (!m_dhash || !m_chash || m_regs[0].m_update || m_regs[1].m_update)
	{
		m_dhash = UpdateHash(nullptr, 0);
		m_chash = UpdateHash(nullptr, 0);

		std::vector<uint8_t> changes;
		changes.reserve(2 * 25 * 2);

		for (int c = 0; c < 2; ++c)
		{
			for (int i = 0; i < 25; ++i)
			{
				if (m_regs[c].m_diff[i])
				{
					changes.push_back((c << 7) | i);
					changes.push_back(m_regs[c].m_data[i]);
				}
			}

			m_dhash = UpdateHash(m_regs[c].m_data, 25, m_dhash);
			m_regs[c].m_update = false;
		}
		m_chash = UpdateHash(changes.data(), changes.size(), m_chash);
	}
}

bool Frame::Registers::GetInfo(Register reg, Info& info) const
{
	if (reg < 32)
	{
		uint8_t index = (IsExpMode()
			? c_regDefines[reg].expIndex
			: c_regDefines[reg].comIndex);

		if (index != 0xFF)
		{
			info.flags = (index & 0xE0);
			info.index = (index & 0x1F);
			info.mask  = (IsExpMode()
				? c_regDefines[reg].expMask
				: c_regDefines[reg].comMask);
			return true;
		}
	}
	return false;
}

void Frame::Registers::ResetData()
{
	memset(m_data, 0x00, sizeof(m_data));
	m_update = true;
}

void Frame::Registers::ResetChanges()
{
	memset(m_diff, 0x00, sizeof(m_diff));
	m_update = true;
}

void Frame::Registers::SetChanges()
{
	memset(m_diff, 0xFF, sizeof(m_diff));
	m_update = true;
}

bool Frame::Registers::HasChanges() const
{
	for (Register reg = Register::BankA_Fst; reg <= Register::BankB_Lst; ++reg)
	{
		if (IsChanged(reg)) return true;
	}
	return false;
}

void Frame::Registers::ResetExpMode()
{
	uint8_t data = (m_data[c_modeBankRegIdx] & 0x0F);
	Update(Register::Mode_Bank, data);
}

void Frame::Registers::SetExpMode()
{
	uint8_t data = ((m_data[c_modeBankRegIdx] & 0x0F) | 0xA0);
	Update(Register::Mode_Bank, data);
}

bool Frame::Registers::IsExpMode() const
{
	return ((m_data[c_modeBankRegIdx] & 0xE0) == 0xA0);
}

const uint8_t Frame::Registers::tmask(int chan) const
{
	return (0x01 << chan);
}

const uint8_t Frame::Registers::nmask(int chan) const
{
	return (0x08 << chan);
}

const uint8_t Frame::Registers::tmask() const
{
	return 0x07;
}

const uint8_t Frame::Registers::nmask() const
{
	return 0x38;
}

const uint8_t Frame::Registers::emask() const
{
	return (IsExpMode() ? 0x20 : 0x10);
}

const uint8_t Frame::Registers::vmask() const
{
	return (IsExpMode() ? 0x1F : 0x0F);
}

uint8_t Frame::Registers::Read(Register reg) const
{
	Info info;
	if (GetInfo(reg, info))
	{
		if ((info.flags & 0x80) && !m_diff[info.index])
		{
			return c_unchangedShape;
		}
		return m_data[info.index];
	}
	return 0x00;
}

uint16_t Frame::Registers::Read(PRegister preg) const
{
	uint16_t data = 0;
	switch (preg)
	{
	case PRegister::A_Period:  case PRegister::B_Period:  case PRegister::C_Period:
	case PRegister::EA_Period: case PRegister::EB_Period: case PRegister::EC_Period:
		data |= Read(preg + 1) << 8;
		[[fallthrough]];
	case PRegister::N_Period:
		data |= Read(preg + 0);
	}
	return data;
}

void Frame::Registers::Read(int chan, Channel& data) const
{
	if (chan >= 0 && chan <= 2)
	{
		bool isExpMode = IsExpMode();
			
		data.tFine   = Read(Register::t_fine  [chan]);
		data.tCoarse = Read(Register::t_coarse[chan]);
		data.tDuty   = Read(Register::t_duty  [chan]);
		data.mixer   = Read(Register::Mixer) >> chan & 0x09;
		data.volume  = Read(Register::volume [chan]);
		data.eFine   = Read(isExpMode ? Register::e_fine  [chan] : Register::E_Fine);
		data.eCoarse = Read(isExpMode ? Register::e_coarse[chan] : Register::E_Coarse);
		data.eShape  = Read(isExpMode ? Register::e_shape [chan] : Register::E_Shape);
	
		if (data.eShape != c_unchangedShape && isExpMode)
		{
			data.eShape &= 0x0F;
			data.eShape |= 0xA0;
		}
	}
}

bool Frame::Registers::IsChanged(Register reg) const
{
	return IsChanged(reg, 0xFF);
}

bool Frame::Registers::IsChanged(Register reg, uint8_t mask) const
{
	Info info;
	return (GetInfo(reg, info) ? (m_diff[info.index] & mask) : false);
}

bool Frame::Registers::IsChanged(PRegister preg) const
{
	bool changed = false;
	switch (preg)
	{
	case PRegister::A_Period : case PRegister::B_Period : case PRegister::C_Period :
	case PRegister::EA_Period: case PRegister::EB_Period: case PRegister::EC_Period:
		changed |= IsChanged(preg + 1);
		[[fallthrough]];
	case PRegister::N_Period:
		changed |= IsChanged(preg + 0);
	}
	return changed;
}

void Frame::Registers::Update(Register reg, uint8_t data)
{
	Info info;
	if (GetInfo(reg, info))
	{
		// special case for the envelope shape register
		if ((info.flags & 0x80))
		{
			// the envelope shape register can be overwritten with
			// the same value, which resets the envelope generator
			if (data != c_unchangedShape)
			{
				data &= info.mask;
				if (info.index == c_modeBankRegIdx)
				{
					// check if mode changed, reset registers
					if ((m_data[info.index] ^ data) & 0xE0)
					{
						ResetData();
						SetChanges();
					}
				}

				// write new value in register
				m_diff[info.index] = 0xFF;
				m_data[info.index] = data;
				m_update = true;
			}
		}
		else
		{
			// as for the remaining registers, only updating their
			// values ​​has an effect on the sound generation
			data &= info.mask;
			m_diff[info.index] = (m_data[info.index] ^ data);
			m_data[info.index] = data;
			m_update = true;
		}
	}
}

void Frame::Registers::Update(PRegister preg, uint16_t data)
{
	switch (preg)
	{
	case PRegister::A_Period : case PRegister::B_Period : case PRegister::C_Period :
	case PRegister::EA_Period: case PRegister::EB_Period: case PRegister::EC_Period:
		Update(preg + 1, data >> 8);
		[[fallthrough]];
	case PRegister::N_Period:
		Update(preg + 0, uint8_t(data));
	}
}

void Frame::Registers::Update(int chan, const Channel& data)
{
	if (chan >= 0 && chan <= 2)
	{
		bool isExpMode = IsExpMode();
		auto mixerData = Read(Register::Mixer) & ~(0x09 << chan);

		Update(Register::t_fine  [chan], data.tFine);
		Update(Register::t_coarse[chan], data.tCoarse);
		Update(Register::t_duty  [chan], data.tDuty);
		Update(Register::Mixer, mixerData | data.mixer << chan);
		Update(Register::volume  [chan], data.volume);
		Update(Register::e_fine  [chan], data.eFine);
		Update(Register::e_coarse[chan], data.eCoarse);
		Update(Register::e_shape [chan], data.eShape);
	}
}

uint8_t Frame::Registers::GetData(Register reg) const
{
	Info info;
	return (GetInfo(reg, info) ? m_data[info.index] : 0);
}

uint8_t Frame::Registers::GetDiff(Register reg) const
{
	Info info;
	return (GetInfo(reg, info) ? m_diff[info.index] : 0);
}

bool Frame::Registers::IsToneEnabled(int chan) const
{
	return !(GetData(Register::Mixer) & tmask(chan));
}

bool Frame::Registers::IsNoiseEnabled(int chan) const
{
	return !(GetData(Register::Mixer) & nmask(chan));
}

bool Frame::Registers::IsEnvelopeEnabled(int chan) const
{
	return (GetData(Register::volume[chan]) & emask());
}

bool Frame::Registers::IsPeriodicEnvelope(int chan) const
{
	uint8_t shape = (GetData(IsExpMode() ? Register::e_shape[chan] : Register::E_Shape) & 0x0F);
	return (shape == 0x08 || shape == 0x0A || shape == 0x0C || shape == 0x0E);
}

uint8_t Frame::Registers::GetVolume(int chan) const
{
	return (GetData(Register::volume[chan]) & vmask());
}

const Frame::Registers& Frame::operator[](int chip) const
{
	return m_regs[bool(chip)];
}

Frame::Registers& Frame::operator[](int chip)
{
	return m_regs[bool(chip)];
}
