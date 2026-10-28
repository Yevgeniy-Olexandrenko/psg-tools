#include <thread>
#include "Emulator.h"

Emulator::Emulator()
{
}

Emulator::~Emulator()
{
    DeviceClose();
}

const std::string Emulator::GetDeviceName() const
{
    return "Emulator";
}

bool Emulator::DeviceOpen()
{
    return WaveAudio::Open(k_PCMSampleRate, 2, 4, 366);
}

bool Emulator::DeviceInit(const Stream& stream, Chip& dchip)
{
    WaveAudio::Pause();
    PCMOutput::PCMInit(dchip);
    WaveAudio::Resume();
    return true;
}

void Emulator::FillBuffer(std::vector<float>& buffer)
{
    PCMOutput::PCMFill(buffer);
}

void Emulator::DeviceClose()
{
    WaveAudio::Close();
}
