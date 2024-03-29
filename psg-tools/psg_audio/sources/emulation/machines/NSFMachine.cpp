#include "NSFMachine.h"

#define APU_CLOCK_NTSC 1789773
#define APU_CLOCK_PAL  1662607

enum { CPU_RAM, PRG_RAM, MAPPER_REGS, SOLID_ROM, MAPPED_ROM };

bool NSFMachine::Init(const Header& header)
{
	// Check signature
	if (memcmp(header.id, "NESM\x1A", 5))
	{
		return false;
	}
    // Check version
    if (header.version != 0x01)
    {
        return false;
    }
    // Not support any extension yet
    if (header.ext_sound_type != 0)
    {
        return false;
    }
    // NSF seems valid
//    c->music_offset = sizeof(nsf_header_t);
//    c->music_length = reader->size(reader->self) - c->music_offset;
//    c->reader = reader;
    // Format
    if (header.pal_ntsc_bits & 0x02)
    {
        m_format = false;  // Dual NTSC/PAL format, we will use NTSC
    }
    else
    {
        if (header.pal_ntsc_bits & 0x01)
        {
            m_format = true;   // PAL
        }
        else
        {
            m_format = false;  // NTSC
        }
    }
    // Playback rate
    if (m_format)
    {
        m_playback_rate = header.pal_speed;
        m_clock_rate = APU_CLOCK_PAL;
    }
    else
    {
        m_playback_rate = header.ntsc_speed;
        m_clock_rate = APU_CLOCK_NTSC;
    }

    // TODO

    // RAM
    memset(m_cpuRAM, 0, sizeof(m_cpuRAM));
    GetBus().AddWriteReadHandler(this, 0x0000, 0x07FF, CPU_RAM);
    memset(m_prgRAM, 0, sizeof(m_prgRAM));
    GetBus().AddWriteReadHandler(this, 0x6000, 0x7FFF, PRG_RAM);
    // ROM from .nsf file
    m_bank_switched = false;   // check if NSF file uses bank switch
    for (int i = 0; i < 8; ++i)
    {
        if (header.bankswitch_info[i])
        {
            m_bank_switched = true;
            break;
        }
    }
    if (!m_bank_switched)
    {
        // Non bank-switched NSF rom, music data from c->music is loaded to c->header->load_addr
        GetBus().AddReadHandler(this, header.load_addr, header.load_addr + m_music_length - 1, SOLID_ROM);
    }
    else
    {
        // Bank switch register
        GetBus().AddWriteHandler(this, 0x5FF8, 0x5FFF, MAPPER_REGS);
        // Bank switched NSF rom can extend to full address range
        GetBus().AddReadHandler(this, 0x8000, 0xFFFF, MAPPED_ROM);
        // Use bankswitch_info in the header to initialize bank switch register
        for (int i = 0; i < 8; ++i)
        {
            GetBus().Write(0x5FF8 + i, header.bankswitch_info[i]);
        }
    }
    
    // TODO

	return true;
}

void NSFMachine::OnBusWrite(int tag, Addr addr, Data data)
{
    switch (tag)
    {
    case CPU_RAM: m_cpuRAM[addr] = data; break;
    case PRG_RAM: m_prgRAM[addr - 0x6000] = data; break;

    case MAPPER_REGS:
        //
        break;
    }
}

Machine::Data NSFMachine::OnBusRead(int tag, Addr addr) const
{
    switch (tag)
    {
    case CPU_RAM: return m_cpuRAM[addr];
    case PRG_RAM: return m_prgRAM[addr - 0x6000];

    case SOLID_ROM:
        //
        break;

    case MAPPED_ROM:
        //
        break;
    }
    return Machine::Bus::Handler::OnBusRead(tag, addr);
}

