#pragma once

#include <unordered_map>
#include <vector>

template <typename T, size_t DICT_SIZE> 
class LZ78Encoder 
{
    struct LZ78PairHash
    {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2>& p) const
        {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };

public:
    using EncodedData = std::pair<int, T>;

    LZ78Encoder() { Reset(); }

    void Reset()
    {
        m_dictionary.clear();
        m_dictionary[{0, T()}] = 0;
        m_dictIndex = 1;
        m_lastIndex = 0;
    }

    bool Encode(const T& symbol) 
    {
        if (m_dictIndex >= DICT_SIZE) Reset();
        m_encodedData = std::make_pair(m_lastIndex, symbol);

        if (m_dictionary.find(m_encodedData) == m_dictionary.end())
        {
            m_dictionary[m_encodedData] = m_dictIndex++;
            m_lastIndex = 0;
            return true;
        }
        else 
        {
            m_lastIndex = m_dictionary[m_encodedData];
            return false;
        }
    }

    bool HasUnfinishedEncoding() const
    {
        return (m_lastIndex > 0);
    }

    const EncodedData& GetEncodedData() const
    {
        return m_encodedData;
    }

private:
    std::unordered_map<EncodedData, int, LZ78PairHash> m_dictionary;
    EncodedData m_encodedData;
    int m_dictIndex;
    int m_lastIndex;
};

template <typename T, size_t DICT_SIZE> 
class LZ78Decoder
{
public:
    using EncodedData = std::pair<int, T>;
    using DecodedData = std::vector<T>;

    LZ78Decoder() { Reset(); }

    void Reset()
    {
        m_dictionary.clear();
        m_dictIndex = 1;
    }

    DecodedData Decode(const EncodedData& encodedData)
    {
        int index = encodedData.first;
        T symbol = encodedData.second;

        DecodedData prefix = m_dictionary[index];
        prefix.push_back(symbol);

        m_dictionary[m_dictIndex++] = prefix;
        if (m_dictIndex >= DICT_SIZE) Reset();
        return prefix;
    }

private:
    std::unordered_map<int, DecodedData> m_dictionary;
    int m_dictIndex;
};
