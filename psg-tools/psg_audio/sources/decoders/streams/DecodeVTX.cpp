#include "DecodeVTX.h"
#include "lha/lh5.h"

namespace
{
	const uint16_t AYSignature = 0x7961; // "ay"
	const uint16_t YMSignature = 0x6D79; // "ym"
}

bool DecodeVTX::Open(Stream& stream)
{
	bool isDetected = false;
	std::ifstream fileStream;
	fileStream.open(stream.file, std::fstream::binary);

	if (fileStream)
	{
		fileStream.seekg(0, fileStream.end);
		uint32_t fileSize = (uint32_t)fileStream.tellg();

		if (fileSize > 20)
		{
			Header hdr; // data size here
			fileStream.seekg(0, fileStream.beg);
			fileStream.read((char*)(&hdr), sizeof(hdr));

			Chip::Model chipType(Chip::Model::Unknown);
			if (hdr.signature == AYSignature) chipType = Chip::Model::AY8910;
			if (hdr.signature == YMSignature) chipType = Chip::Model::YM2149;
			
			if (chipType != Chip::Model::Unknown)
			{
				stream.schip.first.model = chipType;
				stream.schip.clockValue = hdr.chipFreq;
				stream.play.frameRate = hdr.frameFreq;

				if (hdr.stereo == Stereo::MONO)
				{
					stream.schip.output = Chip::Output::Mono;
				}
				else
				{
					stream.schip.output = Chip::Output::Stereo;
					switch (hdr.stereo)
					{
					case Stereo::ABC: stream.schip.stereo = Chip::Stereo::ABC; break;
					case Stereo::ACB: stream.schip.stereo = Chip::Stereo::ACB; break;
					case Stereo::BAC: stream.schip.stereo = Chip::Stereo::BAC; break;
					case Stereo::BCA: stream.schip.stereo = Chip::Stereo::BCA; break;
					case Stereo::CAB: stream.schip.stereo = Chip::Stereo::CAB; break;
					case Stereo::CBA: stream.schip.stereo = Chip::Stereo::CBA; break;
					}
				}

				auto GetTextProperty = [](std::ifstream& stream)
				{
					std::string str;
					if (stream) std::getline(stream, str, char(0x00));
					return str;
				};

				// read all properties to move forward on stream
				std::string title   = GetTextProperty(fileStream);
				std::string author  = GetTextProperty(fileStream);
				std::string program = GetTextProperty(fileStream); // store in extras
				std::string tracker = GetTextProperty(fileStream); // store in extras
				std::string comment = GetTextProperty(fileStream);

				stream.info.title = title;
				stream.info.artist = author;
				stream.info.comment = comment;
				stream.info.type = "VTX stream";

				// unpack frames data
				uint32_t packedSize = fileSize - (uint32_t)fileStream.tellg();
				std::vector<uint8_t> packedData(packedSize);

				if (fileStream.read((char*)packedData.data(), packedSize))
				{
					m_data.clear();
					m_data.reserve(hdr.dataSize);

					if (lh5::Decompress(packedData.data(), packedSize, m_data) == lh5::Result::Ok)
						isDetected = (m_data.size() == hdr.dataSize && hdr.dataSize >= 14);
				}

				// prepare information on frames
				m_frames = (hdr.dataSize / 14);
				m_loop = hdr.loop;
				m_frame = 0;
			}
		}
		fileStream.close();
	}
	return isDetected;
}

bool DecodeVTX::Decode(Frame& frame)
{
	if (m_frame >= m_frames) return false;
	const uint8_t* dataPtr = (m_data.data() + m_frame);
	m_frame++;

	for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
	{
		frame[0].Update(reg, *dataPtr);
		dataPtr += m_frames;
	}

	return true;
}

void DecodeVTX::Close(Stream& stream)
{
	stream.Finalize(m_loop);
	m_data.clear();
}
