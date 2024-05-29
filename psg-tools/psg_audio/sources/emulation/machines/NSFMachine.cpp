#include "NSFMachine.h"

#define APU_CLOCK_NTSC 1789773
#define APU_CLOCK_PAL  1662607

bool NSFMachine::PowerOn(const Header& header)
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
    m_cpuRAM.AttachToBus(GetBus());
    m_prgRAM.AttachToBus(GetBus());
  
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
        GetBus().AttachRDHandler(header.load_addr, header.load_addr + m_music_length - 1, &NSFMachine::OnSolidRomRead, this);
    }
    else
    {
        // Bank switch register
        GetBus().AttachWRHandler(0x5FF8, 0x5FFF, &NSFMachine::OnMapperRegWrite, this);
        // Bank switched NSF rom can extend to full address range
        GetBus().AttachRDHandler(0x8000, 0xFFFF, &NSFMachine::OnMappedRomRead, this);
        // Use bankswitch_info in the header to initialize bank switch register
        for (int i = 0; i < 8; ++i)
        {
            GetBus().Write(0x5FF8 + i, header.bankswitch_info[i]);
        }
    }
    
    // TODO

    return true;
}

void NSFMachine::OnSolidRomRead(addr_t addr, data_t& data)
{
    //
}

void NSFMachine::OnMapperRegWrite(addr_t addr, data_t& data)
{
    //
}

void NSFMachine::OnMappedRomRead(addr_t addr, data_t& data)
{
    //
}
