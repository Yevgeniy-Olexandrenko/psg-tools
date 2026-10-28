#include <fstream>
#include <filesystem>

#include "argparse/argparse.hpp"
#include "magic_enum/magic_enum.hpp"

#include "FilelistPlayer.h"
#include "FilelistConverter.h"
#include "ConsoleGUI.h"
#include "UITest.h"

static Termination m_termination = false;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    m_termination = true;
    Sleep(10000);
    return TRUE;
}

void PrintWellcome()
{
    gui::Init(L"PSG Tools");
    gui::PrintRuler();
    gui::Print(" PSG Tools v1.0", FG_RED);
    gui::Print(" by Yevgeniy Olexandrenko", FG_RED);
    gui::PrintRuler();
}

#if _DEBUG
void PrintOutputDebugInfo(const std::string& debugInfo)
{
    if (!debugInfo.empty())
    {
        std::string line;
        std::stringstream ss(debugInfo);
        while (getline(ss, line)) gui::Print(' ' + line, FG_DARK_CYAN);
        gui::PrintRuler();
    }
}
#endif

void PrintPlayerControls()
{
    gui::Print(" SPACE\t- Pause/Resume current song playback");
    gui::Print(" DOWN\t- Go to next song in playlist");
    gui::Print(" UP\t- Go to previous song in playlist");
    gui::Print(" F\t- Add/Remove current song to/from favorites");
}

void InitProgramArguments(argparse::ArgumentParser& program)
{
    program.add_argument("input")
        .help("input file(s) or directory")
        .default_value<std::vector<std::string>>({ "favorites.m3u" })
        .nargs(argparse::nargs_pattern::any);

    program.add_argument("-o", "--output")
        .help("output file or output type")
        .metavar("PATH or TYPE");

    program.add_argument("-p", "--port")
        .help("AYM Streamer COM port index")
        .metavar("INDEX")
        .scan<'i', int>();

    program.add_argument("-w", "--wave")
        .help("record playback to WAV file")
        .metavar("PATH");

    program.add_argument("--shuffle")
        .help("random shuffle input playlist")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--favorites")
        .help("user defined favorites playlist")
        .metavar("PATH")
        .default_value(std::string{ "favorites.m3u" });

    program.add_argument("--chip-model")
        .help("destination chip model(s)")
        .nargs(1, 2);

    program.add_argument("--chip-clock")
        .help("destination chip clock rate, MHz")
        .scan<'g', float>();

    program.add_argument("--chip-output")
        .help("destination chip output type (and stereo type)")
        .nargs(1, 2);

    program.add_argument("--ui-test")
        .help("check console UI rendering, without playback")
        .default_value(false)
        .implicit_value(true);
}

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("PSG Tools", "1.0");
    InitProgramArguments(program);

    try { program.parse_args(argc, argv); }
    catch (const std::runtime_error& err) 
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (program.get<bool>("ui-test"))
    {
        SetConsoleCtrlHandler(CtrlHandler, TRUE);
        RunUITest(m_termination);
        return 0;
    }

    // parse list of input files
    FilelistTraversal filelist(FileDecoder::FileTypes);
    auto inputs{ program.get<std::vector<std::string>>("input") };
    for (const auto& path : inputs) filelist.Append(path);

    // parse output file or output type
    std::filesystem::path output;
    if (auto optional = program.present("output")) 
    {
        output = optional.value();
    }

    // parse index of COM port for AYM Streamer
    int comPortIndex = -1;
    if (auto optional = program.present<int>("port"))
    {
        comPortIndex = optional.value();
    }

    // parse path of WAV file to record playback
    std::filesystem::path wavePath;
    if (auto optional = program.present("wave"))
    {
        wavePath = optional.value();
    }

    if (comPortIndex >= 0 && !wavePath.empty())
    {
        std::cerr << "Options --port and --wave can't be used together" << std::endl;
        std::exit(1);
    }

    // parse random shuffle input playlist
    bool isRandomShuffle = program.get<bool>("shuffle");

    // parse list of favorites files
    auto path = program.get("favorites");
    if (!std::filesystem::exists(path))
    {
        std::ofstream file(path);
        file.close();
    }
    Filelist favorites(FileDecoder::FileTypes, path);

    // setup destination chip config
    Chip chip;
    if (auto optional = program.present<std::vector<std::string>>("chip-model"))
    {
        auto values = optional.value();
        if (auto fmodel = magic_enum::enum_cast<Chip::Model>(values[0]))
        {
            chip.first.model = fmodel.value();
        }
        if (values.size() > 1)
        {
            if (auto smodel = magic_enum::enum_cast<Chip::Model>(values[1]))
            {
                chip.second.model = smodel.value();
            }
        }
    }
    if (auto optional = program.present<float>("chip-clock"))
    {
        auto clockRate = int(1e6f * optional.value());
        chip.clockValue = clockRate;
    }
    if (auto optional = program.present<std::vector<std::string>>("chip-output"))
    {
        auto values = optional.value();
        if (auto output = magic_enum::enum_cast<Chip::Output>(values[0]))
        {
            chip.output = output.value();
        }
        if (values.size() > 1)
        {
            if (auto stereo = magic_enum::enum_cast<Chip::Stereo>(values[1]))
            {
                chip.stereo = stereo.value();
            }
        }
    }

    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    PrintWellcome();

    // the console gets its state back however the work ends
    int result = 0;
    try
    {
        if (!filelist.IsEmpty())
        {
            if (!output.empty())
            {
                FilelistConverter converter(chip, filelist, output, m_termination);
                converter.Convert();
            }
            else
            {
                // setup output device
                std::shared_ptr<Output> output;
                if (comPortIndex >= 0)
                    output.reset(new Streamer(comPortIndex));
                else if (!wavePath.empty())
                {
                    if (wavePath.has_parent_path())
                        std::filesystem::create_directories(wavePath.parent_path());
                    output.reset(new WaveFile(wavePath));
                }
                else
                    output.reset(new Emulator());

                // setup and start playback
                if (isRandomShuffle) filelist.RandomShuffle();
                FilelistPlayer player(chip, *output, filelist, favorites, m_termination);
#if _DEBUG
                if (comPortIndex >= 0)
                {
                    const std::string debugInfo = static_cast<Streamer&>(*output).GetDebugInfo();
                    PrintOutputDebugInfo(debugInfo);
                }
#endif
                PrintPlayerControls();
                player.Play();
            }
        }
    }
    catch (const std::exception& e)
    {
        gui::EraseFile();
        gui::Print(std::string("Error: ") + e.what(), FG_RED);
        result = 1;
    }

    gui::Shutdown();
    return result;
}
