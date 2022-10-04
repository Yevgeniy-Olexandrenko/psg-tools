#include "PSGLib.h"

// module decoders
#include "decoders/modules/DecodeASC.h"
#include "decoders/modules/DecodePT2.h"
#include "decoders/modules/DecodePT3.h"
#include "decoders/modules/DecodeSQT.h"
#include "decoders/modules/DecodeSTC.h"
#include "decoders/modules/DecodeSTP.h"

// stream decoders
#include "decoders/streams/DecodePSG.h"
#include "decoders/streams/DecodeVGM.h"
#include "decoders/streams/DecodeVTX.h"
#include "decoders/streams/DecodeYM.h"

// specific encoders
#include "encoders/specific/EncodeTXT.h"
#include "encoders/specific/EncodeWAV.h"

// stream encoders
#include "encoders/streams/EncodeTST.h"
#include "encoders/streams/EncodeAYM.h"
#include "encoders/streams/EncodePSG.h"
#include "encoders/streams/EncodeVGM.h"
#include "encoders/streams/EncodeVTX.h"
#include "encoders/streams/EncodeYM.h"

const std::string PSGHandler::DecodeFileTypes{ "asc|pt2|pt3|sqt|stc|stp|psg|vgm|vgz|vtx|ym" };
const std::string PSGHandler::EncodeFileTypes{ "psg" };

bool PSGHandler::Decode(const std::filesystem::path& path, Stream& stream)
{
	std::shared_ptr<Decoder> decoders[]
	{
	    // modules
		std::shared_ptr<Decoder>(new DecodeASC()),
		std::shared_ptr<Decoder>(new DecodePT2()),
	    std::shared_ptr<Decoder>(new DecodePT3()),
		std::shared_ptr<Decoder>(new DecodeSQT()),
	    std::shared_ptr<Decoder>(new DecodeSTC()),
	    std::shared_ptr<Decoder>(new DecodeSTP()),
	    
	    // streams
	    std::shared_ptr<Decoder>(new DecodePSG()),
	    std::shared_ptr<Decoder>(new DecodeVGM()),
		std::shared_ptr<Decoder>(new DecodeVTX()),
		std::shared_ptr<Decoder>(new DecodeYM ()),
	};
	
	stream.file = path;
	for (std::shared_ptr<Decoder> decoder : decoders)
	{
	    if (decoder->Open(stream))
	    {
	        Frame frame;
	        while (decoder->Decode(frame))
	        {
	            stream.AddFrame(frame);
	            frame.ResetChanges();

				OnFrameDecoded(stream, stream.lastFrameId());
	        }
	
	        decoder->Close(stream);
	        return true;
	    }
	}
	return false;
}

bool PSGHandler::Encode(const std::filesystem::path& path, Stream& stream)
{
	std::shared_ptr<Encoder> encoders[]
	{
	    std::shared_ptr<Encoder>(new EncodeTST()),
	    std::shared_ptr<Encoder>(new EncodePSG()),
	    std::shared_ptr<Encoder>(new EncodeAYM()),
	    std::shared_ptr<Encoder>(new EncodeTXT()),
	};
	
	stream.file = path;
	for (std::shared_ptr<Encoder> encoder : encoders)
	{
	    if (encoder->Open(stream))
	    {
	        for (FrameId id = 0; id < stream.framesCount(); ++id)
	        {
	            const Frame& frame = stream.GetFrame(id);
	            encoder->Encode(frame);

				OnFrameEncoded(stream, id);
	        }
	
	        encoder->Close(stream);
	        return true;
	    }
	}
	return false;
}
