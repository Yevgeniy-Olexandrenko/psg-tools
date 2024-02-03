#pragma once

#include <vector>
#include "FrameProcessor.h"

#define DBG_PROCESSING 0

class ProcessingChain : public FrameProcessor
{
public:
	ProcessingChain(const std::string& tag);
	virtual ~ProcessingChain();

	template <class T> ProcessingChain& Append(const Chip& dstChip);
	template <class T> ProcessingChain& Append(const Chip& srcChip, const Chip& dstChip);
	FrameProcessor* GetProcessor(size_t index) const;
	void Clear();

	const Frame& Execute(const Frame& frame) override;
	const Frame& GetFrame() const;

protected:
	std::string m_tag;
	std::vector<FrameProcessor*> m_chain;
};

template<class T>
inline ProcessingChain& ProcessingChain::Append(const Chip& dstChip)
{
	FrameProcessor* processor = new T(dstChip);
	m_chain.push_back(processor);
	return *this;
}

template<class T>
inline ProcessingChain& ProcessingChain::Append(const Chip& srcChip, const Chip& dstChip)
{
	FrameProcessor* processor = new T(srcChip, dstChip);
	m_chain.push_back(processor);
	return *this;
}
