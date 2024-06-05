#pragma once

#include <stdint.h>

class Register
{
public:
    using Value = uint8_t;

    enum
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

    enum class Period
    {
        A  = Register::A_Fine,
        B  = Register::B_Fine,
        C  = Register::C_Fine,
        N  = Register::N_Period,
        E  = Register::E_Fine,
        EA = Register::EA_Fine,
        EB = Register::EB_Fine,
        EC = Register::EC_Fine
    };

    static const Value t_fine[];
    static const Value t_coarse[];
    static const Value t_duty[];
    static const Value volume[];
    static const Value e_fine[];
    static const Value e_coarse[];
    static const Value e_shape[];

    static const Period t_period[];
    static const Period e_period[];

public:
    Register(Value val) : m_value(val) {}
    operator Value& () { return m_value; }

    static Value Fine(Period regp)   { return (0 + static_cast<Value>(regp)); }
    static Value Coarse(Period regp) { return (1 + static_cast<Value>(regp)); }

private:
    Value m_value;
};
