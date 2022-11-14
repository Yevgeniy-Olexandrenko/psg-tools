#pragma once

#include "decoders/Decoder.h"

class VT2
{
public:
    template <typename T> class List
    {
    public:
        T& add(const T& t) { m_data.push_back(t); return m_data.back(); }
        T& add() { m_data.emplace_back(); return m_data.back(); }

        const T& operator[](size_t pos) const { return m_data[pos]; }
        size_t size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        void loop(size_t loop) { m_loop = loop; }
        size_t loop() const { return m_loop; }

    private:
        std::vector<T> m_data;
        size_t m_loop;
    };

    using OrnamentLine = int;
    using SampleLine = struct
    {
        bool t, n, e;
        int  toneVal;
        bool toneAcc;
        int  noiseVal;
        bool noiseAcc;
        int  volumeVal;
        int  volumeAdd;
    };
    using PatternLine = struct
    {
        struct Chan
        {
            int note;
            int sample;
            int eshape;
            int ornament;
            int volume;

            int cmdType;
            int cmdDelay;
            int cmdParam;
        };

        int  etone;
        int  noise;
        Chan chan[3];
    };

    using Ornament = List<OrnamentLine>;
    using Sample   = List<SampleLine>;
    using Pattern  = List<PatternLine>;
   
    struct Module
    {
        bool isVT2 = false;
        std::string version;
        std::string title;
        std::string author;
        int noteTable = 0;
        int chipFreq = 0;
        int intFreq = 0;
        int speed = 0;

        List<int> positions;
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
    int  GetToneFromNote(int note);

public:
    std::vector<Module> modules;
};

class DecodeVT2 : public ModuleDecoder
{
    struct Channel
    {
        uint8_t ornamentIdx;
        uint8_t ornamentLoop;
        uint8_t ornamentLen;
        uint8_t ornamentPos;

        uint8_t sampleIdx;
        uint8_t sampleLoop;
        uint8_t sampleLen;
        uint8_t samplePos;

        uint8_t volume;
        uint8_t note;
        uint8_t slideToNote;

        int volumeSliding;
        int noiseSliding;
        int envelopeSliding;
        int tonSlideCount;
        int currentOnOff;
        int onOffDelay;
        int offOnDelay;
        int tonSlideDelay;
        int toneSliding;
        int toneAcc;
        int tonSlideStep;
        int toneDelta;

        bool simpleGliss;
        bool envelopeEnabled;
        bool enabled;
    };

    struct Global
    {
        uint8_t envBaseLo;
        uint8_t envBaseHi;

        int curEnvSlide;
        int envSlideAdd;

        int8_t curEnvDelay;
        int8_t envDelay;

        uint8_t noiseBase;
        uint8_t noiseAdd;
    };

    struct Module
    {
        uint8_t m_delay;
        uint8_t m_delayCounter;
        uint8_t m_currentPosition;
        uint8_t m_patternIdx;
        uint8_t m_patternPos;
        Channel m_channels[3];
        Global  m_global;
    };

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    bool PlayModule(int m);
    void ProcessPattern(int m, int c, uint8_t& shape);
    void ProcessInstrument(int m, int c, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& mixer, int8_t& envAdd);
    int  GetToneFromNote(int m, int note);

private:
    VT2 m_vt2;
    int m_version;
    Module m_module[2];
};
