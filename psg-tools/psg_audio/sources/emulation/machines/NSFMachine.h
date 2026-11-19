#pragma once

#include "emulation/Machine.h"
#include "emulation/RP2A03Cpu.h"

// TODO: the machine of the NSF files (the next format): the RP2A03 CPU, 2K of
// RAM, the ROM in 4K banks (the registers 0x5FF8-0x5FFF switch them through
// the page table of the bus), the APU registers 0x4000-0x4017 go to SimRP2A03.
class NSFMachine : public Machine
{
public:
    // Spec: https://wiki.nesdev.org/w/index.php/NSF
    #pragma pack(push, 1)
    struct Header
    {
        uint8_t  id[5];              // 00 NESM\x1A
        uint8_t  version;            // 05 spec version
        uint8_t  num_songs;          // 06 total num songs
        uint8_t  start_song;         // 07 first song
        uint16_t load_addr;          // 08 loc to load code
        uint16_t init_addr;          // 0A init call address
        uint16_t play_addr;          // 0C play call address
        uint8_t  song_name[32];      // 0E name of song
        uint8_t  artist_name[32];    // 2E artist name
        uint8_t  copyright[32];      // 4E copyright info
        uint16_t ntsc_speed;         // 6E playback speed (if NTSC)
        uint8_t  bankswitch_info[8]; // 70 initial code banking
        uint16_t pal_speed;          // 78 playback speed (if PAL)
        uint8_t  pal_ntsc_bits;      // 7A NTSC/PAL determination bits
        uint8_t  ext_sound_type;     // 7B type of external sound gen
        uint8_t  reserved[4];        // 7C reserved
    };
    #pragma pack(pop)
};
