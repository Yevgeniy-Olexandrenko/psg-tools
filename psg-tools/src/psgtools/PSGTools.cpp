#include <argparse.hpp>
#include <magic_enum.hpp>

#include "PlayFiles.h"
#include "ConvertFiles.h"
#include "interface/ConsoleGUI.h"

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

    using namespace terminal;
    std::cout << ' ' << color::bright_blue << std::string(gui::k_consoleWidth - 2, '-') << std::endl;
    std::cout << ' ' << color::bright_red << "PSG Tools v1.0" << std::endl;
    std::cout << ' ' << color::bright_red << "by Yevgeniy Olexandrenko" << std::endl;
    std::cout << ' ' << color::bright_blue << std::string(gui::k_consoleWidth - 2, '-') << std::endl;
    std::cout << color::reset;
}

#if _DEBUG
void PrintOutputDebugInfo(const std::string& debugInfo)
{
    using namespace terminal;
    if (!debugInfo.empty())
    {
        std::string line;
        std::stringstream ss(debugInfo);
        while (getline(ss, line)) std::cout << ' ' << color::cyan << line << std::endl;
        std::cout << ' ' << color::bright_blue << std::string(gui::k_consoleWidth - 2, '-') << std::endl;
        std::cout << color::reset;
    }
}
#endif

void PrintPlayerControls()
{
    using namespace terminal;
    std::cout << " ENTER\t- Pause/Resume current song playback" << std::endl;
    std::cout << " DOWN\t- Go to next song in playlist" << std::endl;
    std::cout << " UP\t- Go to previous song in playlist" << std::endl;
    std::cout << " F\t- Add/Remove current song to/from favorites" << std::endl;
    std::cout << color::reset;
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

    // parse list of input files
    Filelist filelist(PSGHandler::DecodeFileTypes);
    auto inputs{ program.get<std::vector<std::string>>("input") };
    for (const auto& path : inputs) filelist.Append(path);

    // TODO: parse output file or output type
    std::filesystem::path outputPath;
    if (auto optional = program.present("output")) 
    {
        outputPath = optional.value();
    }
    bool isConverting = false;

    // parse index of COM port for AYM Streamer
    int comPortIndex = -1;
    if (auto optional = program.present<int>("port"))
    {
        comPortIndex = optional.value();
    }

    // parse random shuffle input playlist
    bool isRandomShuffle = program.get<bool>("shuffle");

    // parse list of favorites files
    Filelist favorites(PSGHandler::DecodeFileTypes, program.get("favorites"));

    // setup destination chip config
    Chip chip;
    if (auto optional = program.present<std::vector<std::string>>("chip-model"))
    {
        auto values = optional.value();
        if (auto fmodel = magic_enum::enum_cast<Chip::Model>(values[0]))
        {
            chip.first.model(fmodel.value());
        }
        if (values.size() > 1)
        {
            if (auto smodel = magic_enum::enum_cast<Chip::Model>(values[1]))
            {
                chip.second.model(smodel.value());
            }
        }
    }
    if (auto optional = program.present<float>("chip-clock"))
    {
        auto clockRate = int(1e6f * optional.value());
        chip.clockValue(clockRate);
    }
    if (auto optional = program.present<std::vector<std::string>>("chip-output"))
    {
        auto values = optional.value();
        if (auto output = magic_enum::enum_cast<Chip::Output>(values[0]))
        {
            chip.output(output.value());
        }
        if (values.size() > 1)
        {
            if (auto stereo = magic_enum::enum_cast<Chip::Stereo>(values[1]))
            {
                chip.stereo(stereo.value());
            }
        }
    }

    PrintWellcome();
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    if (!filelist.IsEmpty())
    {
        if (isConverting)
        {
            //ConvertInputFiles(outputPath);
        }
        else
        {
            // setup output device
            std::shared_ptr<Output> output;
            if (comPortIndex >= 0)
                output.reset(new Streamer(comPortIndex));
            else
                output.reset(new Emulator());

            // setup and start playback
            if (isRandomShuffle) filelist.RandomShuffle();
            PlayFiles player(chip, *output, filelist, favorites, m_termination);
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
    return 0;
}
