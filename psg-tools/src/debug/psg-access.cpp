// -----------------------------------------------------------------------------
// PSG Access via PIO
// -----------------------------------------------------------------------------

#include <string.h>
#include "psg-access.h"

// helpers
#define set_bit(reg, bit) reg |=  (1 << (bit))
#define res_bit(reg, bit) reg &= ~(1 << (bit))
#define set_bits(reg, bits) reg |=  (bits)
#define res_bits(reg, bits) reg &= ~(bits)
#define isb_set(reg, bit) bool((reg) & (1 << (bit)))
#define isb_res(reg, bit) !bool((reg) & (1 << (bit)))

enum Hash
{
    NotFound     = 0x67E019C7,
    Compatible   = 0xF56B7047,
    AY8910A      = 0x2CFF954F,
    AY8912A      = 0x11111111, // TODO: not implemented
    AY8913A      = 0x22222222, // TODO: not implemented
    AY8930       = 0x4EFE6E06,
    YM2149F      = 0x1D750557,
    AVRAY_FW26   = 0x33333333  // TODO: not implemented
};

PSG::PSG()
    : m_hash(Hash::NotFound)
{
}

uint32_t PSG::s_rclock = 0;
uint32_t PSG::s_vclock = 0;

// -----------------------------------------------------------------------------
// Low Level Interface
// -----------------------------------------------------------------------------

void PSG::Init()
{
    m_hash = Hash::AY8930;

    // setup clock and reset
    if (!s_rclock) SetClock(F1_77MHZ);
    Reset();
}

void PSG::Reset()
{
    m_soundChipReg = 0x00;
    reset_input_state();
}

void PSG::Address(uint8_t reg)
{
    m_soundChipReg = reg;
}

void PSG::Write(uint8_t data)
{
    m_output->emplace_back(m_soundChipReg, data);
}

void PSG::SetClock(uint32_t clock)
{
    if (clock >= F1_00MHZ && clock <= F2_00MHZ)
    {
        s_rclock = clock;
        s_vclock = clock;
    }
}

uint32_t PSG::GetClock()
{
#if defined(PSG_PROCESSING) && defined(PSG_CLOCK_CONVERSION)
    return s_vclock;
#else
    return s_rclock;
#endif
}

// -----------------------------------------------------------------------------
// High Level Interface
// -----------------------------------------------------------------------------

enum { INPUT = 0, OUTPUT = 1 };

PSG::Type PSG::GetType() const
{
    switch (m_hash)
    {
    case Hash::NotFound:   return Type::NotFound;
    case Hash::Compatible: return Type::Compatible;
    case Hash::AY8910A:    return Type::AY8910A;
    case Hash::AY8912A:    return Type::AY8912A;
    case Hash::AY8913A:    return Type::AY8913A;
    case Hash::AY8930:     return Type::AY8930;
    case Hash::YM2149F:    return Type::YM2149F;
    case Hash::AVRAY_FW26: return Type::AVRAY_FW26;
    }
    return Type::BadOrUnknown;
}

bool PSG::IsReady() const
{
    Type type = GetType();
    return (type != Type::NotFound && type != Type::BadOrUnknown);
}

void PSG::SetStereo(Stereo stereo)
{
#if defined(PSG_PROCESSING) && defined(PSG_CHANNELS_REMAPPING)
    m_sstereo = stereo;
    m_dstereo = stereo;
#endif
}

PSG::Stereo PSG::GetStereo() const
{
    return m_dstereo;
}

// set register data indirectly via bank switching
void PSG::SetRegister(uint8_t reg, uint8_t data)
{
    State& state = m_states[m_sindex];

    // register number must be in range 0x00-0x0F
    if (reg < 0x10)
    {
        // redirect access to registers of bank B if
        // exp mode is active and bank B is selected
        if (state.status.exp_mode == 0xB0)
        {
            reg += BankB_Fst;
        }
        SetRegister(Reg(reg), data);
    }
}

// get register data indirectly via bank switching
void PSG::GetRegister(uint8_t reg, uint8_t& data) const
{
    const State& state = m_states[m_sindex];

    // register number must be in range 0x00-0x0F
    if (reg < 0x10)
    {
        // redirect access to registers of bank B if
        // exp mode is active and bank B is selected
        if (state.status.exp_mode == 0xB0)
        {
            reg += BankB_Fst;
        }
        GetRegister(Reg(reg), data);
    }
}

// set register data directly
void PSG::SetRegister(Reg reg, uint8_t  data)
{
    State& state = m_states[m_sindex];

    // preserve the state of exp mode and bank of regs
    // separately from the shape of channel A envelope
    if ((uint8_t(reg) & 0x0F) == Mode_Bank)
    {
        state.status.exp_mode = (data & 0xF0);
        data &= 0x0F; reg = Reg::Mode_Bank;
    }

    // map register to corresponding field of state
    switch(reg)
    {
        // bank A
        case Reg::A_Fine:    state.channels[0].t_period.fine = data; break;
        case Reg::A_Coarse:  state.channels[0].t_period.coarse = data; break;
        case Reg::B_Fine:    state.channels[1].t_period.fine = data; break;
        case Reg::B_Coarse:  state.channels[1].t_period.coarse = data; break;
        case Reg::C_Fine:    state.channels[2].t_period.fine = data; break;
        case Reg::C_Coarse:  state.channels[2].t_period.coarse = data; break;
        case Reg::N_Period:  state.commons.n_period = data; break;
        case Reg::Mixer:     state.commons.mixer = data; break;
        case Reg::A_Volume:  state.channels[0].t_volume = data; break;
        case Reg::B_Volume:  state.channels[1].t_volume = data; break;
        case Reg::C_Volume:  state.channels[2].t_volume = data; break;
        case Reg::EA_Fine:   state.channels[0].e_period.fine = data; break;
        case Reg::EA_Coarse: state.channels[0].e_period.coarse = data; break;
        case Reg::EA_Shape:  state.channels[0].e_shape = data; break;

        // bank B
        case Reg::EB_Fine:   state.channels[1].e_period.fine = data; break;
        case Reg::EB_Coarse: state.channels[1].e_period.coarse = data; break;
        case Reg::EC_Fine:   state.channels[2].e_period.fine = data; break;
        case Reg::EC_Coarse: state.channels[2].e_period.coarse = data; break;
        case Reg::EB_Shape:  state.channels[1].e_shape = data; break;
        case Reg::EC_Shape:  state.channels[2].e_shape = data; break;
        case Reg::A_Duty:    state.channels[0].t_duty = data; break;
        case Reg::B_Duty:    state.channels[1].t_duty = data; break;
        case Reg::C_Duty:    state.channels[2].t_duty = data; break;
        case Reg::N_AndMask: state.commons.n_and_mask = data; break;
        case Reg::N_OrMask:  state.commons.n_or_mask = data; break;
    }

    // mark register as changed
    set_bit(state.status.changed, uint8_t(reg));
}

// get register data directly
void PSG::GetRegister(Reg reg, uint8_t& data) const
{
    const State& state = m_states[m_sindex];

    // map register to corresponding field of state
    switch(reg)
    {
        // bank A
        case Reg::A_Fine:    data = state.channels[0].t_period.fine; break;
        case Reg::A_Coarse:  data = state.channels[0].t_period.coarse; break;
        case Reg::B_Fine:    data = state.channels[1].t_period.fine; break;
        case Reg::B_Coarse:  data = state.channels[1].t_period.coarse; break;
        case Reg::C_Fine:    data = state.channels[2].t_period.fine; break;
        case Reg::C_Coarse:  data = state.channels[2].t_period.coarse; break;
        case Reg::N_Period:  data = state.commons.n_period; break;
        case Reg::Mixer:     data = state.commons.mixer; break;
        case Reg::A_Volume:  data = state.channels[0].t_volume; break;
        case Reg::B_Volume:  data = state.channels[1].t_volume; break;
        case Reg::C_Volume:  data = state.channels[2].t_volume; break;
        case Reg::EA_Fine:   data = state.channels[0].e_period.fine; break;
        case Reg::EA_Coarse: data = state.channels[0].e_period.coarse; break;
        case Reg::EA_Shape:  data = state.channels[0].e_shape; break;

        // bank B
        case Reg::EB_Fine:   data = state.channels[1].e_period.fine; break;
        case Reg::EB_Coarse: data = state.channels[1].e_period.coarse; break;
        case Reg::EC_Fine:   data = state.channels[2].e_period.fine; break;
        case Reg::EC_Coarse: data = state.channels[2].e_period.coarse; break;
        case Reg::EB_Shape:  data = state.channels[1].e_shape; break;
        case Reg::EC_Shape:  data = state.channels[2].e_shape; break;
        case Reg::A_Duty:    data = state.channels[0].t_duty; break;
        case Reg::B_Duty:    data = state.channels[1].t_duty; break;
        case Reg::C_Duty:    data = state.channels[2].t_duty; break;
        case Reg::N_AndMask: data = state.commons.n_and_mask; break;
        case Reg::N_OrMask:  data = state.commons.n_or_mask; break;
    }

    // combine the current state of exp mode and bank
    // of regs with the shape of channel A envelope
    if ((uint8_t(reg) & 0x0F) == Mode_Bank)
    {
        data |= state.status.exp_mode;
    }
}

void PSG::Update(Data& output)
{
    m_output = &output;
    m_output->clear();

    if (IsReady() && m_states[INPUT].status.changed)
    {
        m_states[OUTPUT] = m_states[INPUT];
        m_sindex = OUTPUT;

        process_clock_conversion();
        process_channels_remapping();
        process_ay8930_envelope_fix();
        write_output_state();

        m_states[INPUT].status.changed = 0;
        m_sindex = INPUT;
    }
}

// -----------------------------------------------------------------------------
// Privates - Processing output state
// -----------------------------------------------------------------------------

#if defined(PSG_PROCESSING)

#if defined(PSG_CHANNELS_REMAPPING)
static const uint8_t e_fine  [] PROGMEM = { PSG::EA_Fine,   PSG::EB_Fine,   PSG::EC_Fine   };
static const uint8_t e_coarse[] PROGMEM = { PSG::EA_Coarse, PSG::EB_Coarse, PSG::EC_Coarse };
static const uint8_t e_shape [] PROGMEM = { PSG::EA_Shape,  PSG::EB_Shape,  PSG::EC_Shape  };
#endif

void PSG::process_clock_conversion()
{
#if defined(PSG_CLOCK_CONVERSION)
    if (s_rclock != s_vclock)
    {
        State& state = m_states[m_sindex];
        uint16_t t_bound = (state.status.exp_mode ? 0xFFFF : 0x0FFF);
        uint16_t n_bound = (state.status.exp_mode ? 0x00FF : 0x001F);

        // safe period conversion based on clock ratio
        const auto convert_period = [&](uint16_t& period, uint16_t bound)
        {
            uint32_t converted = ((period * (s_rclock >> 8) / (s_vclock >> 9) + 1) >> 1);
            period = uint16_t(converted > bound ? bound : converted);
        };

        // convert tone and envelope periods
        for (int i = 0; i < 3; ++i)
        {
            convert_period(state.channels[i].t_period.full, t_bound);
            convert_period(state.channels[i].e_period.full, 0xFFFF);
        }

        // convert noise period
        uint16_t period = state.commons.n_period;
        convert_period(period, n_bound);
        state.commons.n_period = uint8_t(period);

        // set period registers as changed
        set_bits(state.status.changed,
            1 << A_Fine  | 1 << A_Coarse  | 1 << B_Fine  | 1 << B_Coarse  | 1 << C_Fine  | 1 << C_Coarse  |
            1 << EA_Fine | 1 << EA_Coarse | 1 << EB_Fine | 1 << EB_Coarse | 1 << EC_Fine | 1 << EC_Coarse |
            1 << N_Period);
    }
#endif
}

void PSG::process_channels_remapping()
{
#if defined(PSG_CHANNELS_REMAPPING)
    State& state = m_states[m_sindex];

    // restrict stereo modes available for exp mode
    m_dstereo = (state.status.exp_mode && m_sstereo != Stereo::ABC && m_sstereo != Stereo::ACB)
        ? Stereo::ABC
        : m_sstereo;

    if (m_dstereo != Stereo::ABC)
    {
        const auto swap_register = [&](uint8_t reg_l, uint8_t reg_r)
        {
            uint8_t data_l, data_r;
            GetRegister(reg_l, data_l);
            GetRegister(reg_r, data_r);
            SetRegister(reg_l, data_r);
            SetRegister(reg_r, data_l);
        };

        const auto swap_channels = [&](uint8_t l, uint8_t r)
        {
            // swap tone period and tone volume/envelope enable
            swap_register(A_Fine   + 2 * l, A_Fine   + 2 * r);
            swap_register(A_Coarse + 2 * l, A_Coarse + 2 * r);
            swap_register(A_Volume + l, A_Volume + r);

            if (state.status.exp_mode)
            {
                // swap tone duty cycle and envelope period
                swap_register(A_Duty + l, A_Duty + r);
                swap_register(pgm_read_byte(e_fine + l), pgm_read_byte(e_fine + r));
                swap_register(pgm_read_byte(e_coarse + l), pgm_read_byte(e_coarse + r));

                // swap envelope shape
                uint8_t shape_l = pgm_read_byte(e_shape + l);
                uint8_t shape_r = pgm_read_byte(e_shape + r);
                bool schanged_l = isb_set(state.status.changed, shape_l);
                bool schanged_r = isb_set(state.status.changed, shape_r);
                swap_register(shape_l, shape_r);
                if (schanged_l) set_bit(state.status.changed, shape_r); 
                else res_bit(state.status.changed, shape_r);
                if (schanged_r) set_bit(state.status.changed, shape_l); 
                else res_bit(state.status.changed, shape_l);
            }

            // swap bits in mixer register
            uint8_t msk_l = (0b00001001 << l);
            uint8_t msk_r = (0b00001001 << r);
            uint8_t mix_l = ((state.commons.mixer & msk_l) >> l);
            uint8_t mix_r = ((state.commons.mixer & msk_r) >> r);
            res_bits(state.commons.mixer, msk_l | msk_r);
            set_bits(state.commons.mixer, mix_l << r);
            set_bits(state.commons.mixer, mix_r << l);
            set_bit(state.status.changed, Mixer);
        };

        // swap channels in pairs
        switch(m_dstereo)
        {
        case Stereo::ACB:
            swap_channels(1, 2); // B <-> C
            break;

        case Stereo::BAC:
            swap_channels(0, 1); // A <-> B
            break;

        case Stereo::BCA:
            swap_channels(0, 1); // A <-> B
            swap_channels(1, 2); // B <-> C
            break;

        case Stereo::CAB:
            swap_channels(1, 2); // B <-> C
            swap_channels(0, 1); // A <-> B
            break;

        case Stereo::CBA:
            swap_channels(0, 2); // A <-> C
            break;
        }
    }
#endif
}

void PSG::process_ay8930_envelope_fix()
{
#if defined(PSG_AY8930_ENVELOPE_FIX)
    if (GetType() == Type::AY8930)
    {
        State& state = m_states[m_sindex];
        for (int i = 0; i < 3; ++i)
        {
            uint8_t volume = state.channels[i].t_volume;
            if (state.status.exp_mode) volume >>= 1;

            // get tone, noise and envelope enable flags
            bool t_disable = isb_set(state.commons.mixer, 0 + i);
            bool n_disable = isb_set(state.commons.mixer, 3 + i);
            bool e_enable  = isb_set(volume, 4);

            // special case - pure envelope
            if (e_enable && t_disable && n_disable)
            {
                // fix by enabling inaudible tone
                state.channels[i].t_period.full = 0;
                state.channels[i].t_duty = 0x08;
                res_bit(state.commons.mixer, 0 + i);

                // set registers changes
                set_bits(state.status.changed,
                    1 << (A_Fine + 2 * i) | 1 << (A_Coarse + 2 * i) |
                    1 << (A_Duty + i) | 1 << Mixer);
            }
        }
    }
#endif
}
#endif

// -----------------------------------------------------------------------------
// Privates - Handling input/output states
// -----------------------------------------------------------------------------

void PSG::reset_input_state()
{
    memset(&m_states[0], 0, sizeof(State));
}

void PSG::write_output_state()
{
    const State& state = m_states[m_sindex];
    bool switch_banks = false; uint8_t data;

    if (GetType() == Type::AY8930 && state.status.exp_mode)
    {
        // check for changes in registers of bank B
        for (uint8_t reg = BankB_Fst; reg < BankB_Lst; ++reg)
        {
            if (isb_set(state.status.changed, reg))
            {
                // we have changes, so first
                // of all we switch to bank B
                if (!switch_banks)
                {
                    switch_banks = true;
                    GetRegister(Reg::Mode_Bank, data);
                    data &= 0x0F; data |= 0xB0;
                    Address(Mode_Bank); Write(data);
                }

                // send register data to chip (within bank B)
                GetRegister(Reg(reg), data);
                Address(reg & 0x0F); Write(data);
            }
        }

        if (switch_banks)
        {
            // we wrote something to bank B,
            // so we switch back to bank A
            GetRegister(Reg::Mode_Bank, data);
            data &= 0x0F; data |= 0xA0;
            Address(Mode_Bank); Write(data);
        }
    }

    // check for changes in registers of bank A
    for (uint8_t reg = BankA_Fst; reg <= BankA_Lst; ++reg)
    {
        if (isb_set(state.status.changed, reg))
        {
            // skip the 'mode/bank' register if 
            // we've done a bank switch before
            if (switch_banks && reg == Mode_Bank) continue;

            // send register data to chip (within bank A)
            GetRegister(Reg(reg), data);
            Address(reg & 0x0F); Write(data);
        }
    }
}
