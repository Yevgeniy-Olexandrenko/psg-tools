#pragma once

#include <stdint.h>
#include <string>
#include "utils/Property.h"

struct Chip
{
public:
    enum class Model  { Unknown, AY8910, YM2149, AY8930, Compatible };
    enum class Clock  { Unknown, F1000000, F1750000, F1773400, F2000000 };
    enum class Output { Unknown, Mono, Stereo };
    enum class Stereo { Unknown, ABC, ACB, BAC, BCA, CAB, CBA };

public:
    Chip();
    Chip(const Chip& other);
    Chip& operator=(const Chip& other);

    struct F
    {
        F();

        Property<Model> model;

    private:
        Model m_model;
    } first;

    struct S
    {
        S();

        Property<Model> model;
        ReadOnlyProperty<bool> modelKnown;

    private:
        Model m_model;
    } second;

    ReadOnlyProperty<int>  count;
    Property<Clock>        clock;
    Property<int>          clockValue;
    Property<Output>       output;
    Property<Stereo>       stereo;
    ReadOnlyProperty<bool> clockKnown;
    ReadOnlyProperty<bool> outputKnown;
    ReadOnlyProperty<bool> stereoKnown;

    const Model& model(int index) const;
    bool hasExpMode(int index) const;
    std::string toString() const;

private:
    void SetClockValue(const int& val);
    int  GetClockValue() const;

private:
    Clock  m_clock;
    Output m_output;
    Stereo m_stereo;
};