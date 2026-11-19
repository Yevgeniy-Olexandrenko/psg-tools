#include "TrackPath.h"
#include "stream/Stream.h"

namespace TrackPath
{
	Path Make(const Path& file, int number)
	{
		Path path = file;
		path += (k_numberMark + std::to_string(number));
		return path;
	}

	bool Split(const Path& path, Path& file, int& number)
	{
		// the native (wide) name keeps the characters outside the ANSI code page
		const Path::string_type name = path.filename().native();
		const size_t mark = name.rfind(Path::value_type(k_numberMark));
		if (mark == Path::string_type::npos || mark == 0 || mark + 1 == name.size()) return false;

		int value = 0;
		for (size_t i = mark + 1; i < name.size(); ++i)
		{
			if (name[i] < '0' || name[i] > '9' || value > 9999) return false;
			value = value * 10 + (name[i] - '0');
		}
		if (!value) return false;

		std::error_code error;
		Path candidate = path;
		candidate.replace_filename(Path(name.substr(0, mark)));
		if (std::filesystem::exists(path, error) || !std::filesystem::is_regular_file(candidate, error)) return false;

		file = candidate;
		number = value;
		return true;
	}

	std::string OutputStem(const Path& path, const Stream& stream)
	{
		Path file;
		int number;
		if (!Split(path, file, number)) return path.stem().string();

		// the characters that file names cannot have become '_'
		std::string name = stream.info.title;
		for (char& c : name)
		{
			if (uint8_t(c) < 0x20 || std::string("\\/:*?\"<>|").find(c) != std::string::npos) c = '_';
		}
		while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();

		if (name.empty())
		{
			size_t digits = std::max<size_t>(2, std::to_string(std::max(stream.tracksCount, number)).size());
			name = std::to_string(number);
			name.insert(0, digits - name.size(), '0');
		}
		return file.stem().string() + k_nameSeparator + name;
	}
}
