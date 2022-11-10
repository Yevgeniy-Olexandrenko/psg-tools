#include "DecodeVT2.h"

std::vector<std::string> split(std::string target, std::string delim)
{
	std::vector<std::string> tokens;
	if (!target.empty()) 
	{
		size_t start = 0;
		while (true)
		{
			size_t pos = target.find(delim, start);
			if (pos == std::string::npos) break;
			tokens.push_back(target.substr(start, pos - start));
			start = pos + delim.size();
		};
		tokens.push_back(target.substr(start));
	}
	return tokens;
}

void parse_list(const std::string& str, VT2::List<int>& list)
{
	list.data.clear();
	list.loop = 0;

	std::vector<std::string> tokens = split(str, ",");
	for (int i = 0; i < tokens.size(); ++i)
	{
		const std::string& token = tokens[i];

		int offset = 0;
		if (token[0] == 'L') { list.loop = i; offset = 1; }
		list.data.push_back(std::stoi(token.substr(offset)));
	}
}

void parse_number(const std::string& str, int pos, int len, int base, int& out)
{
	out = std::stoi(str.substr(pos, len), nullptr, base);
}

void parse_number(const std::string& str, int base, int& out)
{
	out = std::stoi(str, nullptr, base);
}

////////////////////////////////////////////////////////////////////////////////

void VT2::Parse(std::istream& stream)
{
	std::string line;
	while (std::getline(stream, line))
	{
		if (line == "[Module]") ParseModule(stream);
		else if (line.find("[Ornament") == 0) ParseOrnament(line, stream);
		else if (line.find("[Sample") == 0) ParseSample(line, stream);
		else if (line.find("[Pattern") == 0) ParsePattern(line, stream);
	}
}

void VT2::ParseModule(std::istream& stream)
{
	modules.emplace_back();
	Module& module = modules.back();

	std::string line;
	while (std::getline(stream, line) && !line.empty())
	{
		size_t pos = line.find_first_of('=');
		if (pos != std::string::npos)
		{
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			if (key == "VortexTrackerII") module.VortexTrackerII = (value != "0");
			else if (key == "Version"   ) module.Version   = value;
			else if (key == "Title"     ) module.Title     = value;
			else if (key == "Author"    ) module.Author    = value;
			else if (key == "NoteTable" ) module.NoteTable = std::stoi(value);
			else if (key == "ChipFreq"  ) module.ChipFreq  = std::stoi(value);
			else if (key == "IntFreq"   ) module.IntFreq   = std::stoi(value);
			else if (key == "Speed"     ) module.Speed     = std::stoi(value);
			else if (key == "PlayOrder" ) parse_list(value, module.PlayOrder);
		}
	}
}

void VT2::ParseOrnament(std::string& line, std::istream& stream)
{
	Module& module = modules.back();
	size_t  index  = std::stoi(line.substr(9, line.length() - 9 - 1));

	if (module.ornaments.size() <= index)
		module.ornaments.resize(index + 1);
	Ornament& ornament = module.ornaments[index];

	if (std::getline(stream, line))
	{
		parse_list(line, ornament.positions);
	}
}

void VT2::ParseSample(std::string& line, std::istream& stream)
{
	Module& module = modules.back();
	size_t  index = std::stoi(line.substr(7, line.length() - 7 - 1));

	if (module.samples.size() <= index)
		module.samples.resize(index + 1);
	Sample& sample = module.samples[index];

	int pos = 0;
	while (std::getline(stream, line) && !line.empty())
	{
		sample.positions.data.emplace_back();
		Sample::Line& sampleLine = sample.positions.data.back();
		std::vector<std::string> tokens = split(line, " ");

		sampleLine.t = (tokens[0][0] == 'T');
		sampleLine.n = (tokens[0][1] == 'N');
		sampleLine.e = (tokens[0][2] == 'E');

		sampleLine.toneNeg = (tokens[1][0] == '-');
		parse_number(tokens[1], 1, 3, 16, sampleLine.toneVal);
		sampleLine.toneAcc = (tokens[1][4] == '^');

		sampleLine.noiseNeg = (tokens[2][0] == '-');
		parse_number(tokens[2], 1, 2, 16, sampleLine.noiseVal);
		sampleLine.noiseAcc = (tokens[2][3] == '^');

		parse_number(tokens[3], 0, 1, 16, sampleLine.volumeVal);
		sampleLine.volumeAdd = 0;
		if (tokens[3][1] == '+') sampleLine.volumeAdd = +1;
		if (tokens[3][1] == '-') sampleLine.volumeAdd = -1;

		if (tokens.size() > 4 && tokens.back() == "L")
		{
			sample.positions.loop = pos;
		}
		pos++;
	}
}

void VT2::ParsePattern(std::string& line, std::istream& stream)
{
	Module& module = modules.back();
	size_t  index = std::stoi(line.substr(8, line.length() - 8 - 1));

	if (module.patterns.size() <= index)
		module.patterns.resize(index + 1);
	Pattern& pattern = module.patterns[index];

	while (std::getline(stream, line) && !line.empty())
	{
		pattern.positions.emplace_back();
		Pattern::Line& patternLine = pattern.positions.back();

		std::replace(line.begin(), line.end(), '.', '0');
		std::vector<std::string> tokens = split(line, "|");

		if (tokens[0][2] == '-' || tokens[0][2] == '#')
		{
			// TODO
		} 
		else
		parse_number(tokens[0], 16, patternLine.etone);
		parse_number(tokens[1], 16, patternLine.noise);

		for (int i = 0; i < 3; ++i)
		{
			Pattern::Line::Chan& chan = patternLine.chan[i];
			const std::string& token = tokens[2 + i];

			chan.note = 0;
			switch (token[0] | token[1] << 8)
			{
			case 'C' | '-' << 8: chan.note = 0x1; break;
			case 'C' | '#' << 8: chan.note = 0x2; break;
			case 'D' | '-' << 8: chan.note = 0x3; break;
			case 'D' | '#' << 8: chan.note = 0x4; break;
			case 'E' | '-' << 8: chan.note = 0x5; break;
			case 'F' | '-' << 8: chan.note = 0x6; break;
			case 'F' | '#' << 8: chan.note = 0x7; break;
			case 'G' | '-' << 8: chan.note = 0x8; break;
			case 'G' | '#' << 8: chan.note = 0x9; break;
			case 'A' | '-' << 8: chan.note = 0xA; break;
			case 'A' | '#' << 8: chan.note = 0xB; break;
			case 'B' | '-' << 8: chan.note = 0xC; break;
			case 'R' | '-' << 8: chan.note =  -1; break;
			}
			if (chan.note > 0)
			{
				chan.note += (token[2] - '1') * 12;
			}

			parse_number(token, 4, 1, 32, chan.sample);
			parse_number(token, 5, 1, 16, chan.eshape);
			parse_number(token, 6, 1, 16, chan.ornament);
			parse_number(token, 7, 1, 16, chan.volume);

			parse_number(token,  9, 1, 16, chan.command);
			parse_number(token, 10, 1, 16, chan.delay);
			parse_number(token, 11, 1, 16, chan.paramH);
			parse_number(token, 12, 1, 16, chan.paramL);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool DecodeVT2::Open(Stream& stream)
{
	bool isDetected = false;
	if (CheckFileExt(stream, "vt2"))
	{
		std::ifstream fileStream;
		fileStream.open(stream.file, std::fstream::binary);

		if (fileStream)
		{
			VT2 vt2;
			vt2.ParseModule(fileStream);

			if (!vt2.modules.empty())
			{
				bool isFileOk = true;
				isFileOk &= !vt2.modules[0].Version.empty();
				isFileOk &=  vt2.modules[0].Speed > 0;
				isFileOk &= !vt2.modules[0].PlayOrder.data.empty();

				if (isFileOk)
				{
					fileStream.seekg(0, fileStream.beg);
					m_vt2.Parse(fileStream);

					Init();
					isDetected = true;

					stream.info.title(m_vt2.modules[0].Title);
					stream.info.artist(m_vt2.modules[0].Author);
					if (m_vt2.modules[0].ChipFreq)
						stream.schip.clockValue(m_vt2.modules[0].ChipFreq);
					if (m_vt2.modules[0].IntFreq)
						stream.play.frameRate((m_vt2.modules[0].IntFreq + 500) / 1000);
					stream.info.type("Vortex Tracker II (PT v" + m_vt2.modules[0].Version + ") module");
				}
			}
			fileStream.close();
		}
	}
	return isDetected;
}

void DecodeVT2::Init()
{
}

void DecodeVT2::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
}

bool DecodeVT2::Play()
{
	return false;
}

void DecodeVT2::InitPattern()
{
}

void DecodeVT2::ProcessPattern(int ch, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
}

void DecodeVT2::ProcessInstrument(int ch, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& noise, uint8_t& mixer)
{
}
