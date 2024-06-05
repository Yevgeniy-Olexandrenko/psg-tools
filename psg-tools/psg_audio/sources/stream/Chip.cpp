#include <sstream>
#include "Chip.h"

namespace
{
    const int k_clocks[] = { 0, 1000000, 1750000, 1773400, 2000000 };
}

Chip::Chip()
    : m_clock(Clock::Unknown)
    , m_output(Output::Unknown)
    , m_stereo(Stereo::Unknown)
    , count([this]() { return (second.modelKnown ? 2 : 1); })
    , clock(m_clock)
    , clockValue([this]() { return this->GetClockValue(); }, [this](const int& val) { this->SetClockValue(val); })
    , output(m_output)
    , stereo(m_stereo)
    , clockKnown([this]()  { return (m_clock  != Clock::Unknown);  })
    , outputKnown([this]() { return (m_output != Output::Unknown); })
    , stereoKnown([this]() { return (m_stereo != Stereo::Unknown); })
{
}

Chip::Chip(const Chip& other)
    : Chip()
{
    *this = other;
}

Chip& Chip::operator=(const Chip& other)
{
    if (this != &other)
    {
        first.model  = other.first.model;
        second.model = other.second.model;
        clock  = other.clock;
        output = other.output;
        stereo = other.stereo;
    }
    return *this;
}

Chip::F::F()
    : m_model(Model::Compatible)
    , model(m_model, [this](const Model& val) { m_model = (val == Model::Unknown ? Model::Compatible : val); })
{
}

Chip::S::S()
    : m_model(Model::Unknown)
    , model(m_model)
    , modelKnown([this]() { return (m_model != Model::Unknown); })
{
}

const Chip::Model& Chip::model(int index) const
{
    return (index ? second.model : first.model);
}

bool Chip::hasExpMode(int index) const
{
    return (model(index) == Model::AY8930);
}

std::string Chip::toString() const
{
    const auto OutputModel = [](std::ostream& stream, Model type)
    {
        switch (type)
        {
        case Model::AY8910:     stream << "AY-3-8910";        break;
        case Model::YM2149:     stream << "YM2149F";          break;
        case Model::AY8930:     stream << "AY8930";           break;
        case Model::Compatible: stream << "AY/YM Compatible"; break;
        }
    };

    std::stringstream stream;
    if (count == 2)
    {
        if (first.model == second.model)
        {
            stream << "2 x ";
            OutputModel(stream, first.model);
        }
        else
        {
            OutputModel(stream, first.model);
            stream << " + ";
            OutputModel(stream, second.model);
        }
        stream << ' ';
    }
    else
    {
        OutputModel(stream, first.model);
        stream << ' ';
    }

    if (clockKnown)
    {
        stream << double(clockValue) / 1000000 << " MHz" << ' ';
    }

    if (outputKnown)
    {
        if (output == Output::Mono)
        {
            stream << "Mono";
        }
        else if (stereoKnown)
        {
            switch (stereo)
            {
            case Stereo::ABC: stream << "ABC"; break;
            case Stereo::ACB: stream << "ACB"; break;
            case Stereo::BAC: stream << "BAC"; break;
            case Stereo::BCA: stream << "BCA"; break;
            case Stereo::CAB: stream << "CAB"; break;
            case Stereo::CBA: stream << "CBA"; break;
            }
        }
        else
        {
            stream << "Stereo";
        }
    }

    return stream.str();
}

void Chip::SetClockValue(const int& val)
{
    auto dist = [&](int i)
    {
        return std::abs(val - k_clocks[i]);
    };
    
    int f = 0;
    for (int i = 1; i < 5; ++i)
    {
        if (dist(i) < dist(f)) f = i;
    }
    m_clock = Clock(f);
}

int Chip::GetClockValue() const
{
    return k_clocks[size_t(m_clock)];
}
