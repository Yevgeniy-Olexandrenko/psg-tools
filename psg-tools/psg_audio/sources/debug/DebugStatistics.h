#pragma once

#if defined(_DEBUG)

#include <cstdint>
#include <vector>
#include <map>

namespace DebugStatistics
{
    using icount_t = uint32_t;
    using iindex_t = uint32_t;
    using findex_t = float;
    using sample_t = float;

    struct Algorithm;
    
    class Accumulator
    {
    public:
        Accumulator(icount_t required = 100);

        const Accumulator& operator+=(sample_t sample);
        const Accumulator& operator+=(const Accumulator& other);

        bool IsStable() const;
        icount_t Size() const;
        sample_t operator[](iindex_t index) const;
        sample_t operator[](findex_t index) const;
        void Sort() const;

        template<typename T>
        sample_t Compute() const;

        template<typename T>
        sample_t Compute(const T& algorithm) const;

        template<>
        sample_t Compute<Algorithm>(const Algorithm& algorithm) const;

    private:
        using AlgorithmId = size_t;
        using InstantiationMarker = size_t;
        sample_t Compute(const Algorithm & algorithm, AlgorithmId algorithmId) const;

    private:
        icount_t m_required;
        mutable icount_t m_nextSize;
        mutable std::vector<sample_t> m_samples;
        mutable std::map<AlgorithmId, sample_t> m_cache;
        mutable bool m_isSorted;
    };

    template<typename T>
    sample_t Accumulator::Compute() const
    {
        const T algorithm;
        return this->Compute<T>(algorithm);
    }

    template<typename T>
    sample_t Accumulator::Compute(const T& algorithm) const
    {
        static InstantiationMarker marker;
        AlgorithmId algorithmId = reinterpret_cast<AlgorithmId>(&marker);
        return this->Compute(algorithm, algorithmId);
    }

    template<>
    inline sample_t Accumulator::Compute<Algorithm>(const Algorithm& algorithm) const
    {
        AlgorithmId algorithmId = reinterpret_cast<AlgorithmId>(&algorithm);
        return this->Compute(algorithm, algorithmId);
    }

    ///////////////////////////////////////////////////////////////////////////////

    //	Five-number summary
    //	https://en.wikipedia.org/wiki/Five-number_summary
    //	https://en.wikipedia.org/wiki/Box_plot

    struct FiveNumberSummary
    {
        FiveNumberSummary(const Accumulator & accumulator);

        sample_t m_min = 0, m_max = 0;
        sample_t m_p25 = 0, m_p50 = 0, m_p75 = 0;

        struct {
            findex_t m_min = 0, m_max = 0;
            findex_t m_p25 = 0, m_p50 = 0, m_p75 = 0;
        } m_index;
    };

    ////////////////////////////////////////////////////////////////////////////

    //	Normal distribution
    //	https://en.wikipedia.org/wiki/Normal_distribution

    struct NormalDistribution
    {
        NormalDistribution(const Accumulator& accumulator);

        sample_t m_mean  = 0;
        sample_t m_sigma = 0;
    };

    ////////////////////////////////////////////////////////////////////////////

    //	Frequency distribution
    //	https://en.wikipedia.org/wiki/Frequency_distribution
    //	https://en.wikipedia.org/wiki/Histogram
    class FrequencyDistribution
    {
    public:
        struct Class
        {
            Class(sample_t begin, sample_t width);

            bool IsInBounds(sample_t sample) const;
            void Increase();

            const sample_t m_lowerBound;
            const sample_t m_upperBound;
            const icount_t m_samples;
        };

        FrequencyDistribution(const Accumulator & accumulator, float scale = 1.f);

        const FiveNumberSummary& Summary() const;
        const Class& operator[](iindex_t index) const;

        icount_t Classes() const;
        icount_t Samples() const;

        float Frequency(iindex_t index) const;
        float Persent(iindex_t index) const;

    private:
        FiveNumberSummary m_summary;
        std::vector<Class> m_distribution;
        icount_t m_samples;
    };

    ////////////////////////////////////////////////////////////////////////////

    struct Algorithm
    {
        virtual sample_t operator()(const Accumulator & accumulator) const = 0;
    };

    //	Arithmetic mean
    //	https://en.wikipedia.org/wiki/Arithmetic_mean
    //	Outliers are excluded during computation.
    struct ArithmeticMeanAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator & accumulator) const override;
    };

    //	Standard deviation
    //	https://en.wikipedia.org/wiki/Standard_deviation
    struct StandardDeviationAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator& accumulator) const override;
    };

    //	Interquartile mean
    //	https://en.wikipedia.org/wiki/Interquartile_mean
    struct InterquartileMeanAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator & accumulator) const override;
    };

    //	Interquartile range
    //	https://en.wikipedia.org/wiki/Interquartile_range
    struct InterquartileRangeAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator& accumulator) const override;
    };

    //	Mid-range
    //	https://en.wikipedia.org/wiki/Mid-range
    struct MidRangeAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator & accumulator) const override;
    };

    //	Mode (statistics)
    //	https://en.wikipedia.org/wiki/Mode_(statistics)
    struct ModeAlgorithm : public Algorithm
    {
        virtual sample_t operator()(const Accumulator & accumulator) const override;
    };	
}

#endif
