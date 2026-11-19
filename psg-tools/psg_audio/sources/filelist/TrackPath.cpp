#include "TrackPath.h"

namespace TrackPath
{
	std::string Number(int number)
	{
		std::string digits = std::to_string(number);
		if (digits.size() < 2) digits.insert(0, 2 - digits.size(), '0');
		return k_numberMark + digits;
	}

	Path Make(const Path& file, int number)
	{
		Path path = file;
		path += Number(number);
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

	std::string OutputStem(const Path& path)
	{
		Path file;
		int number;
		if (!Split(path, file, number)) return path.stem().string();
		return file.stem().string() + k_nameSeparator + Number(number);
	}
}
