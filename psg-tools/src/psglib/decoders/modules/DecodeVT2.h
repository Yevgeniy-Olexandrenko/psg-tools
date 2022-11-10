#pragma once

#include "decoders/Decoder.h"

class VT2
{
public:
    template <typename T> struct List
    {
        const T& operator[](size_t pos) const { return data[pos]; }
        std::vector<T> data;
        size_t loop;
    };

    struct Ornament
    {
        List<int> positions;
    };

    struct Sample
    {
        struct Line
        {
            bool t, n, e;
            bool toneNeg;
            int  toneVal;
            bool toneAcc;
            bool noiseNeg;
            int  noiseVal;
            bool noiseAcc;
            int  volumeVal;
            int  volumeAdd; // 0, -1, +1
        };
        List<Line> positions;
    };

    struct Pattern
    {
        struct Line
        {
            struct Chan
            {
                int note;
                int sample;
                int eshape;
                int ornament;
                int volume;

                int command;
                int delay;
                int paramH;
                int paramL;
            };

            int  etone;
            int  noise;
            Chan chan[3];
        };
        std::vector<Line> positions;
    };

    struct Module
    {
        bool VortexTrackerII = false;
        std::string Version;
        std::string Title;
        std::string Author;
        int NoteTable = 0;
        int ChipFreq = 0;
        int IntFreq = 0;
        int Speed = 0;
        List<int> PlayOrder;

        std::vector<Ornament> ornaments;
        std::vector<Sample>   samples;
        std::vector<Pattern>  patterns;
    };

    
public:
    void Parse(std::istream& stream);
    void ParseModule(std::istream& stream);

private:
    void ParseOrnament(std::string& line, std::istream& stream);
    void ParseSample(std::string& line, std::istream& stream);
    void ParsePattern(std::string& line, std::istream& stream);

public:
    std::vector<Module> modules;
    

};

class DecodeVT2 : public ModuleDecoder
{
    struct Channel
    {
        //uint16_t patternPtr;
        //uint16_t ornamentPtr;
        //uint16_t samplePtr;

        //uint8_t ornamentLoop;
        //uint8_t ornamentLen;
        //uint8_t ornamentPos;
        //uint8_t sampleLoop;
        //uint8_t sampleLen;
        //uint8_t samplePos;
        //uint8_t volume;
        //uint8_t noteSkip;
        //uint8_t note;
        //uint8_t slideToNote;

        //int8_t toneSliding;
        //int8_t toneDelta;
        //int8_t glissType;
        //int8_t glissade;
        //int8_t additionToNoise;
        //int8_t noteSkipCounter;

        //bool envelopeEnabled;
        //bool enabled;
    };

    //static const uint16_t NoteTable[];

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    void InitPattern();
    void ProcessPattern(int ch, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape);
    void ProcessInstrument(int ch, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& noise, uint8_t& mixer);

private:
    VT2 m_vt2;
    //uint8_t m_delay;
    //uint8_t m_delayCounter;
    //uint8_t m_currentPosition;
    //Channel m_channels[3];
};
