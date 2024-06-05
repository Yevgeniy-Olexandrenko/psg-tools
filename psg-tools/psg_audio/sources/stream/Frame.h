#pragma once

#include "Register.h"
#include "debug/DebugPayload.h"

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
        uint16_t Read(Register::Period regp) const;
        void     Read(int chan, Channel& data) const;

        bool IsChanged(Register reg) const;
        bool IsChanged(Register reg, uint8_t mask) const;
        bool IsChanged(Register::Period regp) const;

        void Update(Register reg, uint8_t data);
        void Update(Register::Period regp, uint16_t data);
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
        uint8_t m_data[25]{};
        uint8_t m_diff[25]{};
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
