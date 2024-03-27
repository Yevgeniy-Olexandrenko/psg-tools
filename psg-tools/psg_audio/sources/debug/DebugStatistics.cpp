#include "DebugStatistics.h"

#if defined(_DEBUG)

#include <algorithm>
#include <iterator>
#include <cassert>
#include <cmath>

namespace DebugStatistics
{
	namespace Math
	{
		template<typename T>
		T min(const T& a, const T& b) { return std::min(a, b); }

		template<typename T>
		T max(const T& a, const T& b) { return std::max(a, b); }

		template<typename T>
		T min(const T& a, const T& b, const T& c) {	return min(a, min(b, c)); }

		template<typename T>
		T max(const T& a, const T& b, const T& c) {	return max(a, max(b, c)); }

		template<typename T>
		bool equals(const T& a, const T& b, T tol = std::numeric_limits<T>::epsilon())
		{
			assert(tol >= 0);
			return (std::abs(a - b) <= tol * max(T(1), std::abs(a), std::abs(b)));
		}

		template<typename T>
		T floor(const T& x) { return std::floor(x); }

		template<typename T>
		T ceil(const T& x)  { return std::ceil(x); }

		template<typename T>
		T fract(const T& x) { return (x - floor(x)); }

		template<typename T> 
		T clamp(const T& x, const T& min, const T& max) { return std::min(std::max(x, min), max); }

		template<typename T>
		T lerp(const T& a, const T& b, double t)  { assert(t >= 0 && t <= 1); return T(a * (1.0 - t) + b * t); }

		template<class T>
		T square(const T& x) { return (x * x); }

		template<class T>
		T sqrt(const T& x) { assert(x >= T(0)); return std::sqrt(x); }
	}

	////////////////////////////////////////////////////////////////////////////

	Accumulator::Accumulator(icount_t required)
		: m_required(required)
		, m_nextSize(required)
		, m_isSorted(false)
	{
		m_samples.reserve(m_required);
	}

	const Accumulator & Accumulator::operator+=(sample_t sample)
	{
		m_samples.push_back(sample);
		m_isSorted = false;
		return (*this);
	}

	const Accumulator& Accumulator::operator+=(const Accumulator& other)
	{
		m_samples.reserve(m_samples.size() + other.m_samples.size());
		std::copy(other.m_samples.begin(), other.m_samples.end(), std::back_inserter(m_samples));

		m_isSorted = false;
		return (*this);
	}

	bool Accumulator::IsStable() const
	{
		return Size() >= m_required;
	}

	icount_t Accumulator::Size() const
	{
		return icount_t(m_samples.size());
	}

	sample_t Accumulator::operator[](findex_t index) const
	{
		const auto i0 = iindex_t(Math::floor(index));
		const auto i1 = iindex_t(Math::ceil(index));

		if (i0 < i1 && i1 < m_samples.size())
		{
			auto interpolator = Math::fract(index);
			return Math::lerp(m_samples[i0], m_samples[i1], interpolator);
		}
		return m_samples[i0];
	}

	sample_t Accumulator::operator[](iindex_t index) const
	{
		return m_samples[index];
	}

	void Accumulator::Sort() const
	{
		if (!m_isSorted)
		{
			std::sort(m_samples.begin(), m_samples.end());
			m_isSorted = true;
		}
	}

	sample_t Accumulator::Compute(const Algorithm& algorithm, AlgorithmId algorithmId) const
	{
		if (IsStable())
		{
			if (Size() >= m_nextSize)
			{
				m_nextSize = Size() + Math::max(m_required / 10, icount_t(1));
				m_cache.clear();
			}

			if (m_cache.find(algorithmId) == m_cache.end())
			{
				m_cache[algorithmId] = (algorithm)(*this);
			}
			return m_cache[algorithmId];
		}
		return sample_t(0);
	}

	////////////////////////////////////////////////////////////////////////////

	FiveNumberSummary::FiveNumberSummary(const Accumulator & accumulator)
	{
		if (accumulator.Size() > 0)
		{
			accumulator.Sort();
			const auto last = findex_t(accumulator.Size() - 1);

			m_p25 = accumulator[m_index.m_p25 = findex_t(0.25) * last];
			m_p50 = accumulator[m_index.m_p50 = findex_t(0.50) * last];
			m_p75 = accumulator[m_index.m_p75 = findex_t(0.75) * last];

			const auto lowerWhisker = m_p25 - sample_t(1.5) * (m_p75 - m_p25);
			const auto upperWhisker = m_p75 + sample_t(1.5) * (m_p75 - m_p25);

			for (int i = 0; i <= static_cast<int>(last); ++i)
			{
				if ((m_min = accumulator[iindex_t(i)]) >= lowerWhisker)
				{
					m_index.m_min = findex_t(i);
					break;
				}
			}
			assert(m_min >= lowerWhisker);

			for (int i = static_cast<int>(last); i >= 0; --i)
			{
				if ((m_max = accumulator[iindex_t(i)]) <= upperWhisker)
				{
					m_index.m_max = findex_t(i);
					break;
				}
			}
			assert(m_max <= upperWhisker);
		}
	}

	////////////////////////////////////////////////////////////////////////////

	NormalDistribution::NormalDistribution(const Accumulator& accumulator)
	{
		ArithmeticMeanAlgorithm meanAlgorithm;
		StandardDeviationAlgorithm sigmaAlgorithm;

		m_mean  = meanAlgorithm(accumulator);
		m_sigma = sigmaAlgorithm(accumulator);
	}

	////////////////////////////////////////////////////////////////////////////

	FrequencyDistribution::Class::Class(sample_t begin, sample_t width)
		: m_lowerBound(begin)
		, m_upperBound(begin + width)
		, m_samples(0)
	{
	}

	bool FrequencyDistribution::Class::IsInBounds(sample_t sample) const
	{
		if (m_lowerBound == m_upperBound)
			return (sample == m_lowerBound);
		else
			return (sample >= m_lowerBound && sample < m_upperBound);
	}

	void FrequencyDistribution::Class::Increase()
	{
		const_cast<icount_t&>(m_samples)++;
	}

	FrequencyDistribution::FrequencyDistribution(const Accumulator & accumulator, float scale)
		: m_summary(accumulator)
		, m_samples(0)
	{
		if (accumulator.IsStable())
		{
			auto i = m_summary.m_index.m_min;
			auto l = m_summary.m_index.m_max;

			//	obtaining information about the range of samples
			auto samples = l - i + 1;
			auto minimum = accumulator[i];
			auto maximum = accumulator[l];
			auto v_range = maximum - minimum;

			//	automatic detection of the required number of classes
			auto limit_c = v_range > 0.f ? Math::min(samples, 1000.f) : 1.f;
			auto classes = Math::clamp(scale * Math::sqrt(samples), 1.f, limit_c);
			auto c_count = icount_t(Math::ceil(classes));
			auto c_width = v_range / classes;

			for (iindex_t c = 0; c < c_count; ++c)
			{
				Class distributionClass(minimum + c * c_width, c_width);
				while (i <= l)
				{
					if (distributionClass.IsInBounds(accumulator[i]))
					{
						distributionClass.Increase();
						i++;
					}
					else break;
				}
				m_distribution.push_back(distributionClass);
				m_samples += distributionClass.m_samples;
			}
		}
	}

	const FiveNumberSummary& FrequencyDistribution::Summary() const
	{
		return m_summary;
	}

	const FrequencyDistribution::Class & FrequencyDistribution::operator[](iindex_t index) const
	{
		return m_distribution[index];
	}

	icount_t FrequencyDistribution::Classes() const
	{
		return icount_t(m_distribution.size());
	}

	icount_t FrequencyDistribution::Samples() const
	{
		return m_samples;
	}

	float FrequencyDistribution::Frequency(iindex_t index) const
	{
		return float(m_distribution[index].m_samples) / m_samples;
	}

	float FrequencyDistribution::Persent(iindex_t index) const
	{
		return 100.f * m_distribution[index].m_samples / m_samples;
	}	

	////////////////////////////////////////////////////////////////////////////

	sample_t ArithmeticMeanAlgorithm::operator()(const Accumulator& accumulator) const
	{
		FiveNumberSummary fiveNumberSummary(accumulator);

		if (fiveNumberSummary.m_min != fiveNumberSummary.m_max)
		{
			auto f = fiveNumberSummary.m_index.m_min;
			auto l = fiveNumberSummary.m_index.m_max;

			sample_t sum = 0;
			for (auto i = f; i <= l; ++i) sum += accumulator[i];
			return sum / (l - f + 1);
		}
		return fiveNumberSummary.m_min;
	}

	sample_t StandardDeviationAlgorithm::operator()(const Accumulator& accumulator) const
	{
		FiveNumberSummary fiveNumberSummary(accumulator);

		if (fiveNumberSummary.m_min != fiveNumberSummary.m_max)
		{
			ArithmeticMeanAlgorithm meanAlgorithm;
			auto mean = meanAlgorithm(accumulator);

			auto f = fiveNumberSummary.m_index.m_min;
			auto l = fiveNumberSummary.m_index.m_max;

			sample_t sum = 0;
			for (auto i = f; i <= l; ++i)
			{
				sum += Math::square(accumulator[i] - mean);
			}
			return Math::sqrt(sum / (l - f + 1));
		}
		return 0;
	}

	sample_t InterquartileMeanAlgorithm::operator()(const Accumulator & accumulator) const
	{
		FiveNumberSummary fiveNumberSummary(accumulator);

		if (fiveNumberSummary.m_p25 != fiveNumberSummary.m_p75)
		{
			auto f = fiveNumberSummary.m_index.m_p25;
			auto l = fiveNumberSummary.m_index.m_p75;

			sample_t sum = 0;
			for (auto i = f; i <= l; ++i) sum += accumulator[i];
			return sum / (l - f + 1);
		}
		return fiveNumberSummary.m_p25;
	}

	sample_t InterquartileRangeAlgorithm::operator()(const Accumulator& accumulator) const
	{
		FiveNumberSummary fiveNumberSummary(accumulator);
		return fiveNumberSummary.m_p75 - fiveNumberSummary.m_p25;
	}

	sample_t MidRangeAlgorithm::operator()(const Accumulator & accumulator) const
	{
		FiveNumberSummary fiveNumberSummary(accumulator);
		return sample_t(0.5) * (fiveNumberSummary.m_min + fiveNumberSummary.m_max);
	}

	sample_t ModeAlgorithm::operator()(const Accumulator& accumulator) const
	{
		if (accumulator.Size() > 0)
		{
			FiveNumberSummary fiveNumberSummary(accumulator);

			auto i = fiveNumberSummary.m_index.m_min;
			auto l = fiveNumberSummary.m_index.m_max;

			auto maxValue = accumulator[i];
			auto maxCount = icount_t(1);
			auto curCount = icount_t(1);

			for (i += 1; i <= l; ++i)
			{
				if (Math::equals(accumulator[i], accumulator[i - 1]))
				{
					curCount++;
				}
				else
				{
					if (curCount > maxCount)
					{
						maxCount = curCount;
						maxValue = accumulator[i - 1];
					}
					curCount = 1;
				}
			}

			if (curCount > maxCount)
			{
				maxCount = curCount;
				maxValue = accumulator[l];
			}
			return maxValue;
		}
		return 0;
	}
}

#endif
