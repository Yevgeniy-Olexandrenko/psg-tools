#pragma once

#include <vector>
#include <utility>
#include "Frame.h"

// Use ChipBus in Output and SimAY8910 instead of their own bank switching
#define Enable_ChipBus

// The register writes of one chip as they go on its bus, and back.
//
// A frame keeps the registers flat: in the AY8930 expanded mode, bank B is at
// 0x10-0x1C. The chip has 16 addresses only: R13 selects bank A or bank B,
// and bank B is at 0x00-0x0C too. R13 also holds the mode and the shape of
// envelope A, so each write to R13, a bank switch too, restarts envelope A.
class ChipBus
{
public:
    // the writes to the chip: (address 0x00-0x0F, data)
    using Writes = std::vector<std::pair<uint8_t, uint8_t>>;

    // Forgets the state of the chip. 'hasExpMode' tells if the chip
    // has the AY8930 expanded mode, which Send needs to know.
    void Reset(bool hasExpMode = true);

    // Frame -> chip. Appends the writes that give the chip the changed
    // registers of a frame. The order of the writes:
    // - R13 first if the mode changes: the switch clears the registers;
    // - the changed registers of bank A, but R13;
    // - the changed registers of bank B, after a switch to bank B;
    // - R13 last if it changed or the bank must go back to A: envelope A
    //   restarts after its period is set, as with the AY-3-8910.
    void Send(const Frame::Registers& regs, Writes& writes);

    // Chip -> frame. Puts one write to the chip into the registers of a frame,
    // as the chip takes it: R13 selects the bank, 0x00-0x0C go to the selected
    // bank, the I/O ports are dropped. The flat addresses 0x10-0x1A (PSG files,
    // EncodeVGM, old Furnace exports) go to the frame as they are.
    void Receive(uint8_t addr, uint8_t data, Frame::Registers& regs);

private:
    enum class Mode { Unknown, Compatible, Expanded };

    bool m_hasExpMode = true;
    Mode m_mode = Mode::Unknown; // the mode of the chip after the last Send
    bool m_bankB = false;        // the bank selected by the writes to Receive
};
