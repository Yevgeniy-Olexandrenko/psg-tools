#include "ChipBus.h"

void ChipBus::Reset(bool hasExpMode)
{
    m_hasExpMode = hasExpMode;
    m_mode = Mode::Unknown;
    m_bankB = false;
}

void ChipBus::Send(const Frame::Registers& regs, Writes& writes)
{
    const auto SendRegister = [&](Register reg, uint8_t data)
    {
        writes.emplace_back(uint8_t(reg & 0x0F), data);
    };

    const bool expMode = (m_hasExpMode && regs.IsExpMode());
    const uint8_t modeBank = regs.GetData(Register::Mode_Bank);

    // The mode switch clears the registers of the chip: R13 goes first. The frame has
    // all the registers changed after a switch. The chip mode is unknown at the start.
    const Mode mode = (expMode ? Mode::Expanded : Mode::Compatible);
    if (m_hasExpMode && mode != m_mode)
    {
        SendRegister(Register::Mode_Bank, modeBank);
        m_mode = mode;
    }

    for (Register reg = Register::BankA_Fst; reg < Register::Mode_Bank; ++reg)
    {
        if (regs.IsChanged(reg)) SendRegister(reg, regs.GetData(reg));
    }

    bool bankB = false;
    if (expMode)
    {
        for (Register reg = Register::EB_Fine; reg <= Register::N_OrMask; ++reg)
        {
            if (regs.IsChanged(reg))
            {
                if (!bankB) SendRegister(Register::Mode_Bank, uint8_t(modeBank | 0x10));
                bankB = true;
                SendRegister(reg, regs.GetData(reg));
            }
        }
    }

    // R13 goes last: envelope A restarts after its period is set, and bank A is back
    if (bankB || regs.IsChanged(Register::Mode_Bank))
    {
        SendRegister(Register::Mode_Bank, modeBank);
    }
}

void ChipBus::Receive(uint8_t addr, uint8_t data, Frame::Registers& regs)
{
    if (addr == Register::Mode_Bank || addr == Register::BankB_Lst)
    {
        // 0xB0 in the high nibble: expanded mode, bank B
        m_bankB = ((data & 0xF0) == 0xB0);
        regs.Update(Register::Mode_Bank, data);
    }
    else if (addr < Register::Mode_Bank)
    {
        regs.Update(m_bankB ? Register::Value(Register::BankB_Fst + addr) : addr, data);
    }
    else if (addr >= Register::BankB_Fst && addr <= Register::N_OrMask)
    {
        regs.Update(addr, data);
    }
}
