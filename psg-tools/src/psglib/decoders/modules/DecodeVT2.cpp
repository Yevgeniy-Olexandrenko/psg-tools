#include "DecodeVT2.h"
#include "SharedTables.h"

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

void parse(const std::string& str, VT2::List<int>& list)
{
	list = VT2::List<int>();
	list.loop(0);

	std::vector<std::string> tokens = split(str, ",");
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		int offset = 0;
		if (tokens[i][0] == 'L') { list.loop(i); offset = 1; }
		list.add(std::stoi(tokens[i].substr(offset)));
	}
}

void parse(const std::string& str, int pos, int len, int base, int& out)
{
	out = std::stoi(str.substr(pos, len), nullptr, base);
}

void parse(const std::string& str, int base, int& out)
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
		else if (line.find("[Sample"  ) == 0) ParseSample  (line, stream);
		else if (line.find("[Pattern" ) == 0) ParsePattern (line, stream);
	}
}

void VT2::ParseModule(std::istream& stream)
{
	modules.emplace_back();
	auto& module = modules.back();

	std::string line;
	while (std::getline(stream, line) && !line.empty())
	{
		size_t pos = line.find_first_of('=');
		if (pos != std::string::npos)
		{
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			if (key == "VortexTrackerII") module.isVT2     = (value != "0");
			else if (key == "Version"   ) module.version   = value;
			else if (key == "Title"     ) module.title     = value;
			else if (key == "Author"    ) module.author    = value;
			else if (key == "NoteTable" ) module.noteTable = std::stoi(value);
			else if (key == "ChipFreq"  ) module.chipFreq  = std::stoi(value);
			else if (key == "IntFreq"   ) module.intFreq   = std::stoi(value);
			else if (key == "Speed"     ) module.speed     = std::stoi(value);
			else if (key == "PlayOrder" ) parse(value, module.positions);
		}
	}
}

void VT2::ParseOrnament(std::string& line, std::istream& stream)
{
	auto& module = modules.back();
	size_t index = std::stoi(line.substr(9, line.length() - 9 - 1));

	if (module.ornaments.empty())
	{
		// empty ornament for index = 0
		module.ornaments.emplace_back();
		module.ornaments[0].add(0);
		module.ornaments[0].loop(0);
	}

	if (module.ornaments.size() <= index)
		module.ornaments.resize(index + 1);
	
	if (std::getline(stream, line))
		parse(line, module.ornaments[index]);
}

void VT2::ParseSample(std::string& line, std::istream& stream)
{
	auto& module = modules.back();
	size_t index = std::stoi(line.substr(7, line.length() - 7 - 1));

	if (module.samples.size() <= index)
		module.samples.resize(index + 1);

	for (int pos = 0; std::getline(stream, line) && !line.empty(); ++pos)
	{
		auto& sampleLine = module.samples[index].add();
		std::vector<std::string> tokens = split(line, " ");

		sampleLine.t = (tokens[0][0] == 'T');
		sampleLine.n = (tokens[0][1] == 'N');
		sampleLine.e = (tokens[0][2] == 'E');

		parse(tokens[1], 1, 3, 16, sampleLine.toneVal);
		if (tokens[1][0] == '-') sampleLine.toneVal *= -1;
		sampleLine.toneAcc = (tokens[1][4] == '^');

		parse(tokens[2], 1, 2, 16, sampleLine.noiseVal);
		if (tokens[2][0] == '-') sampleLine.noiseVal *= -1;
		sampleLine.noiseAcc = (tokens[2][3] == '^');

		parse(tokens[3], 0, 1, 16, sampleLine.volumeVal);
		sampleLine.volumeAdd = 0;
		if (tokens[3][1] == '+') sampleLine.volumeAdd = +1;
		if (tokens[3][1] == '-') sampleLine.volumeAdd = -1;

		if (tokens.size() > 4 && tokens.back() == "L")
		{
			module.samples[index].loop(pos);
		}
	}
}

void VT2::ParsePattern(std::string& line, std::istream& stream)
{
	auto& module = modules.back();
	size_t index = std::stoi(line.substr(8, line.length() - 8 - 1));

	if (module.patterns.size() <= index)
		module.patterns.resize(index + 1);

	while (std::getline(stream, line) && !line.empty())
	{
		auto& patternLine = module.patterns[index].add();
		std::replace(line.begin(), line.end(), '.', '0');
		std::vector<std::string> tokens = split(line, "|");

		const auto ParseNote = [](const std::string& str, int offset) -> int
		{
			int note = 0;
			switch (str[offset++] | str[offset++] << 8)
			{
			case 'C' | '-' << 8: note = 0x1; break;
			case 'C' | '#' << 8: note = 0x2; break;
			case 'D' | '-' << 8: note = 0x3; break;
			case 'D' | '#' << 8: note = 0x4; break;
			case 'E' | '-' << 8: note = 0x5; break;
			case 'F' | '-' << 8: note = 0x6; break;
			case 'F' | '#' << 8: note = 0x7; break;
			case 'G' | '-' << 8: note = 0x8; break;
			case 'G' | '#' << 8: note = 0x9; break;
			case 'A' | '-' << 8: note = 0xA; break;
			case 'A' | '#' << 8: note = 0xB; break;
			case 'B' | '-' << 8: note = 0xC; break;
			case 'R' | '-' << 8: note =  -1; break;
			}
			if (note > 0) note += (str[offset] - '1') * 12;
			return note;
		};

		if (tokens[0][2] == '-' || tokens[0][2] == '#')
		{
			int note = ParseNote(tokens[0], 1);
			if (note > 0)
			{
				int period = GetTonePeriod(note - 1);
				patternLine.etone = (period + 8) / 16;
			}
		} 
		else
		parse(tokens[0], 16, patternLine.etone);
		parse(tokens[1], 16, patternLine.noise);

		for (int i = 0; i < 3; ++i)
		{
			PatternLine::Chan& chan  = patternLine.chan[i];
			const std::string& token = tokens[2 + i];

			chan.note = ParseNote(token, 0);
			parse(token,  4, 1, 32, chan.sample);
			parse(token,  5, 1, 16, chan.eshape);
			parse(token,  6, 1, 16, chan.ornament);
			parse(token,  7, 1, 16, chan.volume);
			parse(token,  9, 1, 16, chan.cmdType);
			parse(token, 10, 1, 16, chan.cmdDelay);
			parse(token, 11, 2, 16, chan.cmdParam);
		}
	}
}

int VT2::GetTonePeriod(int note)
{
	int version = modules.back().version[2];
	switch (modules.back().noteTable)
	{
	case  0: return (version <= 3)
		? NoteTable_PT_33_34r[note]
		: NoteTable_PT_34_35[note];
	case  1: return NoteTable_ST[note];
	case  2: return (version <= 3)
		? NoteTable_ASM_34r[note]
		: NoteTable_ASM_34_35[note];
	default: return (version <= 3)
		? NoteTable_REAL_34r[note]
		: NoteTable_REAL_34_35[note];
	}
}

////////////////////////////////////////////////////////////////////////////////

bool DecodeVT2::Open(Stream& stream)
{
	bool isDetected = false;
	if (CheckFileExt(stream, { "vt2", "txt" }))
	{
		std::ifstream fileStream;
		fileStream.open(stream.file);

		if (fileStream)
		{
			VT2 vt2;
			vt2.ParseModule(fileStream);

			if (!vt2.modules.empty())
			{
				bool isFileOk = true;
				isFileOk &= !vt2.modules[0].version.empty();
				isFileOk &=  vt2.modules[0].speed > 0;
				isFileOk &= !vt2.modules[0].positions.empty();

				if (isFileOk)
				{
					fileStream.seekg(0, fileStream.beg);
					m_vt2.Parse(fileStream);

					Init();
					isDetected = true;

					VT2::Module& module = m_vt2.modules[0];
					stream.info.title(module.title);
					stream.info.artist(module.author);
					if (module.chipFreq)
						stream.schip.clockValue(module.chipFreq);
					if (m_isTS)
						stream.schip.second.model(stream.schip.first.model());
					if (module.intFreq)
						stream.play.frameRate((module.intFreq + 500) / 1000);
					stream.info.type("Vortex Tracker II (PT v" + module.version + ") module");
				}
			}
			fileStream.close();
		}
	}
	return isDetected;
}

////////////////////////////////////////////////////////////////////////////////

void DecodeVT2::Init()
{
	uint8_t ver = m_vt2.modules[0].version[2];
	m_version = ('0' <= ver && ver <= '9') ? ver - '0' : 6;
	m_isTS = (m_vt2.modules.size() > 1);

	for (int m = 0; m < m_vt2.modules.size(); ++m)
	{
		auto& vtm = m_vt2.modules[m];
		auto& mod = m_module[m];
	
		mod.m_delay = vtm.speed;
		mod.m_delayCounter = 1;
		mod.m_currentPosition = 0;
		mod.m_patternIdx = vtm.positions[mod.m_currentPosition];
		mod.m_patternPos = 0;

		for (auto& cha : mod.m_channels)
		{
			memset(&cha, 0, sizeof(cha));
			cha.ornamentIdx = 0;
			cha.ornamentLoop = (uint8_t)vtm.ornaments[cha.ornamentIdx].loop();
			cha.ornamentLen = (uint8_t)vtm.ornaments[cha.ornamentIdx].size();
			cha.sampleIdx = 1;
			cha.sampleLoop = (uint8_t)vtm.samples[cha.sampleIdx].loop();
			cha.sampleLen = (uint8_t)vtm.samples[cha.sampleIdx].size();
			cha.volume = 0xF;
		}
	}
	memset(&m_regs, 0, sizeof(m_regs));
}

void DecodeVT2::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
	currPosition = m_module[0].m_currentPosition;
	loopPosition = (uint8_t)m_vt2.modules[0].positions.loop();
	lastPosition = (uint8_t)m_vt2.modules[0].positions.size() - 1;
}

bool DecodeVT2::Play()
{
	bool loop = PlayModule(0);
	if (m_isTS) PlayModule(1);
	return loop;
}

bool DecodeVT2::PlayModule(int m)
{
	bool loop = false;
	auto regs = m_regs[m];
	auto& vtm = m_vt2.modules[m];
	auto& mod = m_module[m];

	if (!--mod.m_delayCounter)
	{
		for (int c = 0; c < 3; ++c)
		{
			auto& cha = mod.m_channels[c];
			if (!c && mod.m_patternPos == vtm.patterns[mod.m_patternIdx].size())
			{
				if (++mod.m_currentPosition == vtm.positions.size())
				{
					mod.m_currentPosition = (uint8_t)vtm.positions.loop();
					loop = true;
				}

				mod.m_patternIdx = vtm.positions[mod.m_currentPosition];
				mod.m_patternPos = 0;
				mod.m_global.noiseBase = 0;
			}
			ProcessPattern(m, c, regs[Register::E_Shape]);
		}
		mod.m_patternPos++;
		mod.m_delayCounter = mod.m_delay;
	}

	regs[Register::Mixer] = 0;
	int8_t envAdd = 0;
	ProcessInstrument(m, 0, (uint16_t&)regs[Register::A_Fine], regs[Register::Mixer], regs[Register::A_Volume], envAdd);
	ProcessInstrument(m, 1, (uint16_t&)regs[Register::B_Fine], regs[Register::Mixer], regs[Register::B_Volume], envAdd);
	ProcessInstrument(m, 2, (uint16_t&)regs[Register::C_Fine], regs[Register::Mixer], regs[Register::C_Volume], envAdd);

	uint16_t envBase = (mod.m_global.envBaseLo | mod.m_global.envBaseHi << 8);
	(uint16_t&)regs[Register::E_Fine] = (envBase + envAdd + mod.m_global.curEnvSlide);
	regs[Register::N_Period] = (mod.m_global.noiseBase + mod.m_global.noiseAdd) & 0x1F;

	if (mod.m_global.curEnvDelay > 0)
	{
		if (!--mod.m_global.curEnvDelay)
		{
			mod.m_global.curEnvDelay = mod.m_global.envDelay;
			mod.m_global.curEnvSlide += mod.m_global.envSlideAdd;
		}
	}

	return loop;
}

void DecodeVT2::ProcessPattern(int m, int c, uint8_t& shape)
{
	auto& vtm = m_vt2.modules[m];
	auto& mod = m_module[m];
	auto& cha = mod.m_channels[c];
	auto& pln = vtm.patterns[mod.m_patternIdx][mod.m_patternPos];

	int PrNote = cha.note;
	int PrSliding = cha.toneSliding;

	// sample
	if (pln.chan[c].sample > 0 && pln.chan[c].note != 0)
	{
		cha.sampleIdx = pln.chan[c].sample;
		cha.sampleLoop = (uint8_t)vtm.samples[cha.sampleIdx].loop();
		cha.sampleLen = (uint8_t)vtm.samples[cha.sampleIdx].size();
	}

	// ornament
	if (pln.chan[c].ornament > 0 || pln.chan[c].eshape > 0x0)
	{
		cha.ornamentIdx = pln.chan[c].ornament;
		cha.ornamentLoop = (uint8_t)vtm.ornaments[cha.ornamentIdx].loop();
		cha.ornamentLen = (uint8_t)vtm.ornaments[cha.ornamentIdx].size();
		cha.ornamentPos = 0;
	}

	// envelope On
	if (pln.chan[c].eshape > 0x0 && pln.chan[c].eshape < 0xF)
	{
		shape = pln.chan[c].eshape;
		mod.m_global.envBaseHi = (pln.etone >> 8 & 0xFF);
		mod.m_global.envBaseLo = (pln.etone & 0xFF);
		mod.m_global.curEnvSlide = 0;
		mod.m_global.curEnvDelay = 0;
		cha.envelopeEnabled = true;
		cha.ornamentPos = 0;
	}

	// envelope Off
	if (pln.chan[c].eshape == 0xF)
	{
		cha.envelopeEnabled = false;
		cha.ornamentPos = 0;
	}

	// noise
	if (c == 1)
	{
		mod.m_global.noiseBase = pln.noise;
	}

	// volume
	if (pln.chan[c].volume > 0)
	{
		cha.volume = pln.chan[c].volume;
	}

	// note On
	if (pln.chan[c].note > 0)
	{
		cha.note = (pln.chan[c].note - 1);
		cha.samplePos = 0;
		cha.volumeSliding = 0;
		cha.noiseSliding = 0;
		cha.envelopeSliding = 0;
		cha.ornamentPos = 0;
		cha.toneSlideCount = 0;
		cha.toneSliding = 0;
		cha.toneAcc = 0;
		cha.currentOnOff = 0;
		cha.enabled = true;
	}

	// note Off
	if (pln.chan[c].note < 0)
	{
		cha.samplePos = 0;
		cha.volumeSliding = 0;
		cha.noiseSliding = 0;
		cha.envelopeSliding = 0;
		cha.ornamentPos = 0;
		cha.toneSlideCount = 0;
		cha.toneSliding = 0;
		cha.toneAcc = 0;
		cha.currentOnOff = 0;
		cha.enabled = false;
	}

	// commands
	switch (pln.chan[c].cmdType)
	{
	case 0x1: // tone slide down
		cha.simpleGliss = true;
		cha.toneSlideDelay = pln.chan[c].cmdDelay;
		cha.toneSlideCount = cha.toneSlideDelay;
		cha.toneSlideStep = +pln.chan[c].cmdParam;
		if (cha.toneSlideCount == 0 && m_version >= 7) cha.toneSlideCount++;
		cha.currentOnOff = 0;
		break;

	case 0x2: // tone slide up
		cha.simpleGliss = true;
		cha.toneSlideDelay = pln.chan[c].cmdDelay;
		cha.toneSlideCount = cha.toneSlideDelay;
		cha.toneSlideStep = -pln.chan[c].cmdParam;
		if (cha.toneSlideCount == 0 && m_version >= 7) cha.toneSlideCount++;
		cha.currentOnOff = 0;
		break;

	case 0x3: // tone portamento
		cha.simpleGliss = false;
		cha.toneSlideDelay = pln.chan[c].cmdDelay;
		cha.toneSlideCount = cha.toneSlideDelay;
		cha.toneSlideStep = pln.chan[c].cmdParam;
		cha.toneDelta = (GetTonePeriod(m, cha.note) - GetTonePeriod(m, PrNote));
		cha.slideToNote = cha.note;
		cha.note = PrNote;
		if (m_version >= 6) cha.toneSliding = PrSliding;
		if (cha.toneDelta - cha.toneSliding < 0) cha.toneSlideStep = -cha.toneSlideStep;
		cha.currentOnOff = 0;
		break;

	case 0x4: // sample offset
		cha.samplePos = pln.chan[c].cmdParam;
		break;

	case 0x5: // ornament offset
		cha.ornamentPos = pln.chan[c].cmdParam;
		break;

	case 0x6: // vibrato
		cha.onOffDelay = pln.chan[c].cmdParam >> 4 & 0xF;
		cha.offOnDelay = pln.chan[c].cmdParam & 0xF;
		cha.currentOnOff = cha.onOffDelay;
		cha.toneSlideCount = 0;
		cha.toneSliding = 0;
		break;

	case 0x9: // envelope slide down
		mod.m_global.envDelay = pln.chan[c].cmdDelay;
		mod.m_global.curEnvDelay = mod.m_global.envDelay;
		mod.m_global.envSlideAdd = +pln.chan[c].cmdParam;
		break;

	case 0xA: // envelope slide up
		mod.m_global.envDelay = pln.chan[c].cmdDelay;
		mod.m_global.curEnvDelay = mod.m_global.envDelay;
		mod.m_global.envSlideAdd = -pln.chan[c].cmdParam;
		break;

	case 0xB: // set speed
		mod.m_delay = pln.chan[c].cmdParam;
		if (m_isTS)
		{
			m_module[0].m_delay = mod.m_delay;
			m_module[0].m_delayCounter = mod.m_delay;
			m_module[1].m_delay = mod.m_delay;
		}
		break;
	}
}

void DecodeVT2::ProcessInstrument(int m, int c, uint16_t& tperiod, uint8_t& mixer, uint8_t& volume, int8_t& envAdd)
{
	auto& vtm = m_vt2.modules[m];
	auto& mod = m_module[m];
	auto& cha = mod.m_channels[c];

	if (cha.enabled)
	{
		const auto& sampleLine = vtm.samples[cha.sampleIdx][cha.samplePos];
		if (++cha.samplePos >= cha.sampleLen) cha.samplePos = cha.sampleLoop;

		const auto& ornamentLine = vtm.ornaments[cha.ornamentIdx][cha.ornamentPos];
		if (++cha.ornamentPos >= cha.ornamentLen) cha.ornamentPos = cha.ornamentLoop;

		uint16_t tone = (sampleLine.toneVal + cha.toneAcc);
		if (sampleLine.toneAcc) cha.toneAcc = tone;

		int8_t note = (cha.note + uint8_t(ornamentLine));
		if (note < 0 ) note = 0;
		if (note > 95) note = 95;

		tone += (cha.toneSliding + GetTonePeriod(m, note));
		tperiod = (tone & 0x0FFF);

		if (cha.toneSlideCount > 0)
		{
			if (!--cha.toneSlideCount)
			{
				cha.toneSliding += cha.toneSlideStep;
				cha.toneSlideCount = cha.toneSlideDelay;
				if (!cha.simpleGliss)
				{
					if ((cha.toneSlideStep < 0 && cha.toneSliding <= cha.toneDelta) ||
						(cha.toneSlideStep >= 0 && cha.toneSliding >= cha.toneDelta))
					{
						cha.note = cha.slideToNote;
						cha.toneSlideCount = 0;
						cha.toneSliding = 0;
					}
				}
			}
		}

		if (cha.volumeSliding > -15 && cha.volumeSliding < +15)
		{
			cha.volumeSliding += sampleLine.volumeAdd;
		}
		int vol = sampleLine.volumeVal + cha.volumeSliding;
		if (vol < 0x0) vol = 0x0;
		if (vol > 0xF) vol = 0xF;

		volume = m_version <= 4 
			? VolumeTable_33_34[cha.volume][vol]
			: VolumeTable_35[cha.volume][vol];
		if (sampleLine.e && cha.envelopeEnabled) volume |= 0x10;

		if (!sampleLine.n)
		{
			uint8_t envelopeSliding = uint8_t(sampleLine.noiseVal) + cha.envelopeSliding;
			if (sampleLine.noiseAcc) cha.envelopeSliding = envelopeSliding;
			envAdd += envelopeSliding;
		}
		else
		{
			mod.m_global.noiseAdd = (sampleLine.noiseVal & 0x1F) + cha.noiseSliding;
			if (sampleLine.noiseAcc) cha.noiseSliding = mod.m_global.noiseAdd;
		}
		mixer |= uint8_t(!sampleLine.n) << 6;
		mixer |= uint8_t(!sampleLine.t) << 3;
	}
	else volume = 0;
	mixer >>= 1;

	if (cha.currentOnOff > 0)
	{
		if (!--cha.currentOnOff)
		{
			cha.enabled ^= true;
			cha.currentOnOff = (cha.enabled ? cha.onOffDelay : cha.offOnDelay);
		}
	}
}

int DecodeVT2::GetTonePeriod(int m, int note)
{
	switch (m_vt2.modules[m].noteTable)
	{
	case  0: return (m_version <= 3)
		? NoteTable_PT_33_34r[note]
		: NoteTable_PT_34_35[note];
	case  1: return NoteTable_ST[note];
	case  2: return (m_version <= 3)
		? NoteTable_ASM_34r[note]
		: NoteTable_ASM_34_35[note];
	default: return (m_version <= 3)
		? NoteTable_REAL_34r[note]
		: NoteTable_REAL_34_35[note];
	}
}
