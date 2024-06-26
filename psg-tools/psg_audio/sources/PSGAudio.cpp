#include "PSGAudio.h"

// module decoders
#include "decoders/modules/DecodeASC.h"
#include "decoders/modules/DecodePSC.h"
#include "decoders/modules/DecodePT2.h"
#include "decoders/modules/DecodePT3.h"
#include "decoders/modules/DecodeSQT.h"
#include "decoders/modules/DecodeSTC.h"
#include "decoders/modules/DecodeSTP.h"
#include "decoders/modules/DecodeVT2.h"

// stream decoders
#include "decoders/streams/DecodePSG.h"
#include "decoders/streams/DecodeRSF.h"
#include "decoders/streams/DecodeVGM.h"
#include "decoders/streams/DecodeVTX.h"
#include "decoders/streams/DecodeYM.h"

// specific encoders
#include "encoders/specific/EncodeWAV.h"

// stream encoders
#include "encoders/streams/EncodeAYM.h"
#include "encoders/streams/EncodePSG.h"
#include "encoders/streams/EncodeRSF.h"
#include "encoders/streams/EncodeVGM.h"
#include "encoders/streams/EncodeVTX.h"
#include "encoders/streams/EncodeYM.h"

// processing
#include "processing/ChipClockRateConvert.h"
#include "processing/ChannelsLayoutChange.h"
#include "processing/AY8930EnvelopeFix.h"

// debug
#include "debug/DebugOutput.h"

const std::string FileDecoder::FileTypes{ "asc|psc|pt2|pt3|ts|sqt|stc|stp|vt2|txt|psg|rsf|vgm|vgz|vtx|ym" };
const std::string FileEncoder::FileTypes{ "psg|rsf" };

static DebugOutput<DBG_DECODING> dbg;

bool FileDecoder::Decode(const std::filesystem::path& path, Stream& stream)
{
	std::shared_ptr<Decoder> decoders[]
	{
		// modules
		std::shared_ptr<Decoder>(new DecodeASC()),
		std::shared_ptr<Decoder>(new DecodePSC()),
		std::shared_ptr<Decoder>(new DecodePT2()),
		std::shared_ptr<Decoder>(new DecodePT3()),
		std::shared_ptr<Decoder>(new DecodeSQT()),
		std::shared_ptr<Decoder>(new DecodeSTC()),
		std::shared_ptr<Decoder>(new DecodeSTP()),
		std::shared_ptr<Decoder>(new DecodeVT2()),

		// streams
		std::shared_ptr<Decoder>(new DecodePSG()),
		std::shared_ptr<Decoder>(new DecodeRSF()),
		std::shared_ptr<Decoder>(new DecodeVGM()),
		std::shared_ptr<Decoder>(new DecodeVTX()),
		std::shared_ptr<Decoder>(new DecodeYM()),
	};

	auto FindDecoderAndDecode = [&]() -> bool
	{
		for (auto decoder : decoders)
		{
			if (decoder->Open(stream))
			{
				Frame frame;
				while (decoder->Decode(frame))
				{
					if (!stream.AddFrame(frame)) break;
					frame.ResetChanges();

					FrameId frameId(stream.lastFrameId);
					OnFrameDecoded(stream, frameId);

					if (IsAbortRequested())
					{
						decoder->Close(stream);
						return false;
					}
				}
				decoder->Close(stream);
				return true;
			}
		}
		return false;
	};

	stream.file = path;
	bool result = FindDecoderAndDecode();

	// debug output
	dbg.open("decode_" + stream.ToString(Stream::Property::Tag), &stream);
	dbg.print_payload(&stream);
	dbg.close(&stream);
	
	return result;
}

bool FileEncoder::Encode(const std::filesystem::path& path, Stream& stream)
{
	std::shared_ptr<Encoder> encoders[]
	{
		std::shared_ptr<Encoder>(new EncodePSG()),
		std::shared_ptr<Encoder>(new EncodeRSF()),
		std::shared_ptr<Encoder>(new EncodeAYM()),
	};

	std::shared_ptr<FrameProcessor> processors[]
	{
		std::shared_ptr<FrameProcessor>(new ChipClockRateConvert(stream.schip, stream.dchip)),
		std::shared_ptr<FrameProcessor>(new ChannelsLayoutChange(stream.dchip)),
		std::shared_ptr<FrameProcessor>(new AY8930EnvelopeFix(stream.dchip)),
	};

	stream.file = path;
	for (auto encoder : encoders)
	{
		if (encoder->Open(stream))
		{
			Frame dframe;
			for (size_t i = 0; i < stream.framesCount; ++i)
			{
				auto frameId = FrameId(i);
				const Frame& sframe = stream.GetFrame(frameId);

				// frame processing
				const Frame* pframe = &sframe;
				for (auto processor : processors)
				{
					pframe = &(*processor).Execute(*pframe);
				}
				dframe.ResetChanges();
				dframe += *pframe;

				// frame encoding
				encoder->Encode(dframe);
				OnFrameEncoded(stream, frameId);
			}
			encoder->Close(stream);
			return true;
		}
	}
	return false;
}
