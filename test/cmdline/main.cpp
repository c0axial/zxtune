#include <sound.h>
#include <player.h>
#include <sound_attrs.h>
#include <player_attrs.h>
#include <convert_parameters.h>

#include <../../lib/sound/mixer.h>
#include <../../lib/sound/renderer.h>
#include <../../lib/io/container.h>

#include <../../supp/sound_backend.h>
#include <../../supp/sound_backend_types.h>


#include <tools.h>
#include <error.h>

#include <boost/crc.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <conio.h>
#include <windows.h>
#elif defined __linux__
#include <termio.h>
#endif

using namespace ZXTune;

#define FILE_TAG 97E5F6ED

namespace
{
  std::ostream& operator << (std::ostream& str, const StringMap& sm)
  {
    for (StringMap::const_iterator it = sm.begin(), lim = sm.end(); it != lim; ++it)
    {
      str << it->first << ": " << it->second << '\n';
    }
    return str;
  }

  std::ostream& operator << (std::ostream& str, const ModulePlayer::Info& info)
  {
    static const std::string CAPABILITIES[] = {
      "CAP_AYM", "CAP_BEEPER", "CAP_SOUNDRIVE", "CAP_FM", "?", "?", "?", "?",
      "CAP_CONTAINER", "CAP_MULTITRACK", "CAP_SCANER", "?", "?", "?", "?", "?",
      "CAP_RAW", "CAP_OUT", "CAP_PSG", "CAP_YM", "CAP_ZX50", "CAP_ZXAY", "CAP_VORTEX", "?"
      "?", "?", "?", "?", "?", "?", "?", "?"};
    str << info.Properties << "Capabilities:";
    const std::string* capTxt(CAPABILITIES);
    for (uint32_t capMsk = info.Capabilities; capMsk; capMsk >>= 1, ++capTxt)
    {
      if (capMsk & 1)
      {
        str << ' ' << *capTxt;
      }
    }
    return str;
  }

  std::ostream& operator << (std::ostream& str, const std::vector<Sound::Backend::Info>& infos)
  {
    for (std::vector<Sound::Backend::Info>::const_iterator it = infos.begin(), lim = infos.end();
      it != lim; ++it)
    {
      str << "--" << std::left << std::setw(16) << it->Key << "-- " << it->Description << std::endl;
    }
    return str;
  }

#ifdef _WIN32
  int GetKey()
  {
    return _kbhit() ? _getch() : 0;
  }

  void MoveUp(std::size_t lines)
  {
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE hdl(GetStdHandle(STD_OUTPUT_HANDLE));
    GetConsoleScreenBufferInfo(hdl, &info);
    info.dwCursorPosition.Y -= lines;
    info.dwCursorPosition.X = 0;
    SetConsoleCursorPosition(hdl, info.dwCursorPosition);
  }
#elif defined __linux__
  int GetKey()
  {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
  }

  void MoveUp(std::size_t lines)
  {
    std::cout << "\r\x1b[" << lines << 'A';
  }
#else
  int GetKey()
  {
    return 0;
  }

  void MoveUp(std::size_t)
  {
  }
#endif

  template<class T>
  T Decrease(T val, T speed)
  {
    return val >= speed ? val - speed : 0;
  }

  void MonoChannel(Sound::ChannelMixer& mixer)
  {
    mixer.Mute = false;
    std::fill(mixer.OutMatrix, ArrayEnd(mixer.OutMatrix), Sound::FIXED_POINT_PRECISION);
  }

  void MuteChannel(Sound::ChannelMixer& mixer)
  {
    mixer.Mute = true;
    std::memset(mixer.OutMatrix, 0, sizeof(mixer.OutMatrix));
  }

  void FillMixerMatrix(const String& mix, std::vector<Sound::ChannelMixer>& matrix)
  {
    if (mix == "*")//mono
    {
      std::for_each(matrix.begin(), matrix.end(), MonoChannel);
      return;
    }
    if (mix.empty() || mix.size() > matrix.size() || mix.end() != std::find_if(mix.begin(), mix.end(), 
      !boost::bind(&in_range<String::value_type>, _1, 'A', 'A' + matrix.size() - 1)))
    {
      throw Error(ERROR_DETAIL, 1, "Invalid mixer matrix");
    }
    std::for_each(matrix.begin(), matrix.end(), MuteChannel);
    for (String::size_type letpos = 0, letters = mix.size(); letpos != letters; ++letpos)
    {
      const unsigned leftWeight = Sound::FIXED_POINT_PRECISION * (letters > 1? 
        (letters - letpos - 1) / (letters - 1) : 1);
      const unsigned rightWeight = Sound::FIXED_POINT_PRECISION * (letters > 1?
        letpos / (letters - 1) : 1);
      const unsigned chan = mix[letpos] - 'A';
      matrix[chan].Mute = false;
      matrix[chan].OutMatrix[0] = std::max<Sound::Sample>(Sound::FIXED_POINT_PRECISION, 
        matrix[chan].OutMatrix[0] + leftWeight);
      matrix[chan].OutMatrix[1] = std::max<Sound::Sample>(Sound::FIXED_POINT_PRECISION, 
        matrix[chan].OutMatrix[1] + rightWeight);
    }
  }

  struct PlaybackContext
  {
    PlaybackContext()
      : CapFilter(CAP_ANY)
      , Silent(false), Quiet(false), Mixer3(new Sound::MixerData), Mixer4(new Sound::MixerData)
      , ConvertMask(0), ConvertParam(0)
    {
      //default parameters
      Parameters.SoundParameters.ClockFreq = 1750000;
      Parameters.SoundParameters.SoundFreq = 44100;
      Parameters.SoundParameters.FrameDurationMicrosec = 20000;
      Parameters.DriverFlags = 3;
      Parameters.BufferInMs = 100;
      //generate mixers
      Mixer3->Preamp = Sound::FIXED_POINT_PRECISION;
      Mixer3->InMatrix.resize(3);
      Mixer3->InMatrix[0].OutMatrix[0] = Sound::FIXED_POINT_PRECISION;
      Mixer3->InMatrix[0].OutMatrix[1] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer3->InMatrix[1].OutMatrix[0] = 66 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer3->InMatrix[1].OutMatrix[1] = 66 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer3->InMatrix[2].OutMatrix[0] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer3->InMatrix[2].OutMatrix[1] = Sound::FIXED_POINT_PRECISION;

      Mixer4->Preamp = Sound::FIXED_POINT_PRECISION;
      Mixer4->InMatrix.resize(4);
      Mixer4->InMatrix[0].OutMatrix[0] = Sound::FIXED_POINT_PRECISION;
      Mixer4->InMatrix[0].OutMatrix[1] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer4->InMatrix[1].OutMatrix[0] = Sound::FIXED_POINT_PRECISION;
      Mixer4->InMatrix[1].OutMatrix[1] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer4->InMatrix[2].OutMatrix[0] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer4->InMatrix[2].OutMatrix[1] = Sound::FIXED_POINT_PRECISION;
      Mixer4->InMatrix[3].OutMatrix[0] = 5 * Sound::FIXED_POINT_PRECISION / 100;
      Mixer4->InMatrix[3].OutMatrix[1] = Sound::FIXED_POINT_PRECISION;
    }

    enum ParseState
    {
      PARSE_ERROR,
      PARSE_EXIT,
      PARSE_CONTINUE
    };

    ParseState Parse(int argc, char* argv[]);
    bool DoPlayback(ModulePlayer::Ptr player);

    //processing
    uint32_t CapFilter;
    //output
    Sound::Backend::Ptr Backend;
    String Filename;
    Sound::Backend::Parameters Parameters;
    bool Silent, Quiet;
    Sound::MixerData::Ptr Mixer3, Mixer4;
    //conversion
    uint32_t ConvertMask;
    Conversion::Parameter* ConvertParam;
    Conversion::RawConvertParam RawParam;
    Conversion::VortexTextParam VtxtParam;
  };

  PlaybackContext::ParseState PlaybackContext::Parse(int argc, char* argv[])
  {
    for (int arg = 1; arg != argc; ++arg)
    {
      const std::string& args(argv[arg]);
      if (args == "--help")
      {
        std::vector<Sound::Backend::Info> infos;
        Sound::EnumerateBackends(infos);
        std::cout << argv[0] << " [parameters] filename\n"
          "Available backends:\n";
        std::cout << infos;
        std::cout <<
          "Backend-specific parameters:\n"
          "--raw             -- produce raw output (file backend)\n"
          "--annotate        -- annotate file (file backend)\n"

          "\nOther parameters:\n"
          "--noscan          -- do not perform detailed scanning\n"
          "--nomultitrack    -- do not perform in-depth containers analyze\n"
          "--nocontainer     -- do not parse containers\n"
          "--silent          -- do not produce any output\n"
          "--quiet           -- do not produce dynamic output\n"
          "--ym              -- use YM PSG\n"
          "--loop            -- loop modules playback\n"
          "--clock value     -- set PSG clock (" << Parameters.SoundParameters.ClockFreq << " default)\n"
          "--sound value     -- set sound frequency (" << Parameters.SoundParameters.SoundFreq << " default)\n"
          "--fps value       -- set framerate (" << 1e6f / Parameters.SoundParameters.FrameDurationMicrosec << " default)\n"
          "--frame value     -- set frame duration in uS (" << Parameters.SoundParameters.FrameDurationMicrosec << " default)\n"
          "--mix3/--mix4 ... -- setup mixers ('A', 'AC', 'ABC' etc, '*' for mono)\n"
          "--fir order,a-b   -- use FIR with order and range from a to b\n"

          "\nConversion:\n"
          "--convert mode... -- convert modules to specified mode\n"
          "mode:\n"
          " raw              -- save raw file version: without player\n"
          " text             -- save in text format\n"

          "\nModes:\n"
          "--help            -- this page\n"
          "--info            -- supported formats list\n";
        return PARSE_EXIT;
      }
      else if (args == "--info")
      {
        std::vector<ModulePlayer::Info> infos;
        GetSupportedPlayers(infos);
        std::cout << "Supported module types:\n";
        for (std::vector<ModulePlayer::Info>::const_iterator it = infos.begin(), lim = infos.end(); it != lim; ++it)
        {
          std::cout << *it << "\n------\n";
        }
        return PARSE_EXIT;
      }
      else if (args == "--noscan")
      {
        CapFilter &= ~CAP_STOR_SCANER;
      }
      else if (args == "--nomultitrack")
      {
        CapFilter &= ~CAP_STOR_MULTITRACK;
      }
      else if (args == "--nocontainer")
      {
        CapFilter &= ~CAP_STOR_CONTAINER;
      }
      else if (args == "--silent")
      {
        Silent = true;
      }
      else if (args == "--quiet")
      {
        Quiet = true;
      }
      else if (args == "--ym")
      {
        Parameters.SoundParameters.Flags |= Sound::PSG_TYPE_YM;
      }
      else if (args == "--loop")
      {
        Parameters.SoundParameters.Flags |= Sound::MOD_LOOP;
      }
      else if (args == "--raw")
      {
        Parameters.DriverFlags |= Sound::RAW_STREAM;
      }
      else if (args == "--annotate")
      {
        Parameters.DriverFlags |= Sound::ANNOTATE_STREAM;
      }
      else if (args == "--clock")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid psg clock freq specified" << std::endl;
          return PARSE_ERROR;
        }
        Parameters.SoundParameters.ClockFreq = string_cast<uint32_t>(argv[++arg]);
      }
      else if (args == "--sound")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid sound freq specified" << std::endl;
          return PARSE_ERROR;
        }
        Parameters.SoundParameters.SoundFreq = string_cast<uint32_t>(argv[++arg]);
      }
      else if (args == "--fps")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid fps specified" << std::endl;
          return PARSE_ERROR;
        }
        const double val = string_cast<double>(argv[++arg]);
        Parameters.SoundParameters.FrameDurationMicrosec = std::size_t(1e6f / std::max(val, 1.0));
      }
      else if (args == "--frame")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid framesize specified" << std::endl;
          return PARSE_ERROR;
        }
        Parameters.SoundParameters.FrameDurationMicrosec = 
          std::max(string_cast<uint32_t>(argv[++arg]), uint32_t(1));
      }
      else if (args == "--mix3" || args == "--mix4")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid mixer specified" << std::endl;
          return PARSE_ERROR;
        }
        FillMixerMatrix(argv[++arg], args == "--mix3" ? Mixer3->InMatrix : Mixer4->InMatrix);
      }
      else if (args == "--fir")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid fir params specified" << std::endl;
          return PARSE_ERROR;
        }
        InStringStream str(argv[++arg]);
        char tmp;
        str >> Parameters.FIROrder >> tmp >> Parameters.LowCutoff >> tmp >> Parameters.HighCutoff;
      }
      else if (args == "--convert")
      {
        if (arg == argc - 1)
        {
          std::cout << "Invalid conversion parameter specified" << std::endl;
          return PARSE_ERROR;
        }
        StringArray params;
        boost::algorithm::split(params, argv[++arg], boost::algorithm::is_any_of(","));
        if (params[0] == "raw")
        {
          ConvertMask = CAP_CONV_RAW;
          ConvertParam = &RawParam;
        }
        else if (params[0] == "text")
        {
          ConvertMask = CAP_CONV_VORTEX;
          ConvertParam = &VtxtParam;
        }
        else
        {
          std::cout << "Unknown conversion mode" << std::endl;
          return PARSE_ERROR;
        }
      }
      else if (args[0] == '-' && args[1] == '-') //test for backend
      {
        Backend = Sound::Backend::Create(args.substr(2));
        if (Backend.get() &&
            arg < argc - 2 && (argv[arg + 1][0] != '-' ||
            (strlen(argv[arg + 1]) > 1 && argv[arg + 1][1] != '-')))
        {
          Parameters.DriverParameters = argv[++arg];
        }
      }
      if (arg == argc - 1)
      {
        Filename = args;
      }
    }
    return PARSE_CONTINUE;
  }

  bool PlaybackContext::DoPlayback(ModulePlayer::Ptr player)
  {
    if (ConvertParam)
    {
      ModulePlayer::Info playerInfo;
      player->GetInfo(playerInfo);
      if (playerInfo.Capabilities & ConvertMask)
      {
        Dump dump;
        player->Convert(*ConvertParam, dump);
        boost::crc_32_type crcCalc;
        crcCalc.process_bytes(&dump[0], dump.size());
        const String& name(string_cast(std::hex, crcCalc.checksum()) + ".bin");
        std::ofstream test(name.c_str(), std::ios::binary);
        test.write(safe_ptr_cast<const char*>(&dump[0]), dump.size());
        std::cout << "converted " << name << std::endl;
      }
      else
      {
        std::cout << "skipped" << std::endl;
      }
      return true;
    }

    Module::Information module;
    player->GetModuleInfo(module);
    Backend->SetPlayer(player);
    Sound::Backend::Parameters params;
    Backend->GetSoundParameters(params);

    params = Parameters;

    switch (module.Statistic.Channels)
    {
    case 3:
      params.Mixer = Mixer3;
      break;
    case 4:
      params.Mixer = Mixer4;
      break;
    default:
      std::cerr << "Invalid channels count" << std::endl;
      return false;
    }
    Backend->SetSoundParameters(params);

    boost::format formatter(
      "Position: %1$2d / %2% (%3%)\n"
      "Pattern:  %4$2d / %5%\n"
      "Line:     %6$2d / %7%\n"
      "Channels: %8$2d / %9%\n"
      "Tempo:    %10$2d / %11%\n"
      "Frame: %12$5d / %13%");

    std::size_t dump[100] = {0};
    bool quit(false);

    Backend->Play();

    if (!Silent)
    {
      std::cout << "Module: \n" << module.Properties;
    }

    for (;;)
    {
      const bool stop(Sound::Backend::STOPPED == Backend->GetState());
      if (!Silent && !Quiet)
      {
        std::size_t frame;
        Module::Tracking track;
        Backend->GetModuleState(frame, track);

        std::cout <<
          (formatter % (track.Position + 1) % module.Statistic.Position % (1 + module.Loop) %
          track.Pattern % module.Statistic.Pattern %
          track.Note % module.Statistic.Note %
          track.Channels % module.Statistic.Channels %
          track.Tempo % module.Statistic.Tempo %
          frame % module.Statistic.Frame);
        Sound::Analyze::ChannelsState state;
        Backend->GetSoundState(state);
        const std::size_t WIDTH = 75;
        const std::size_t HEIGTH = 16;
        const std::size_t LIMIT = std::numeric_limits<Sound::Analyze::LevelType>::max();
        const std::size_t FALLSPEED = 8;
        static char filler[WIDTH + 1];
        for (std::size_t chan = 0; chan != state.size(); ++chan)
        {
          if (state[chan].Enabled)
          {
            dump[std::min(state.size() + 1 + state[chan].Band, ArraySize(dump) - 1)] =
              dump[chan] = state[chan].Level;
          }
        }
        for (std::size_t y = HEIGTH; y; --y)
        {
          for (std::size_t i = 0; i < std::min(WIDTH, ArraySize(dump)) - 1; ++i)
          {
            const std::size_t level(dump[i] * HEIGTH / LIMIT);
            filler[i] = level > y ? '#' : ' ';
            filler[i + 1] = 0;
          }
          std::cout << std::endl << filler;
        }
        std::transform(dump, ArrayEnd(dump), dump,
          std::bind2nd(std::ptr_fun(Decrease<Sound::Analyze::LevelType>), FALLSPEED));
        if (quit || stop)
        {
          std::cout << std::endl;
          break;
        }
        MoveUp(5 + HEIGTH);
      }
      boost::this_thread::sleep(boost::posix_time::milliseconds(20));
      const int key(tolower(GetKey()));
      quit = 'q' == key;
      if (quit || stop)
      {
        if (Silent || Quiet)//actual break after display
        {
          break;
        }
      }
      switch (key)
      {
      case 'p':
        Sound::Backend::STARTED == Backend->GetState() ? Backend->Pause() : Backend->Play();
        break;
      case ' ':
        Backend->Stop();
        break;
      case 'z':
        if (params.Mixer->Preamp)
        {
          --params.Mixer->Preamp;
        }
        break;
      case 'x':
        if (params.Mixer->Preamp < Sound::FIXED_POINT_PRECISION)
        {
          ++params.Mixer->Preamp;
        }
        break;
      }
    }
    Backend->Stop();
    return !quit;
  }
}

int main(int argc, char* argv[])
{
  try
  {
    PlaybackContext context;

    switch (context.Parse(argc, argv))
    {
    case PlaybackContext::PARSE_ERROR:
      return 1;
    case PlaybackContext::PARSE_EXIT:
      return 0;
    default:
      break;
    }

    if (context.Filename.empty())
    {
      std::cout << "Invalid file name specified" << std::endl;
      return 1;
    }

    IO::DataContainer::Ptr source(IO::DataContainer::Create(context.Filename));
    ModulePlayer::Info playerInfo;
    if (!ModulePlayer::Check(context.Filename, *source, playerInfo, context.CapFilter))
    {
      std::cerr << "Unsupported module type" << std::endl;
      return 1;
    }

    if (!context.Backend.get())
    {
#ifdef _WIN32
      context.Backend = Sound::Backend::Create("win32");
#elif defined __linux__
      context.Backend = Sound::Backend::Create("oss");
#else
      context.Backend = Sound::Backend::Create("");//null
#endif
    }

    StringArray filesToPlay;
    {
      ModulePlayer::Ptr player(ModulePlayer::Create(context.Filename, *source, context.CapFilter));
      if (!player.get())
      {
        std::cerr << "Invalid module specified" << std::endl;
        return 1;
      }
      Module::Information module;
      player->GetModuleInfo(module);
      if (module.Capabilities & CAP_STOR_MULTITRACK)
      {
        boost::algorithm::split(filesToPlay, module.Properties[Module::ATTR_SUBMODULES],
          boost::algorithm::is_cntrl());
      }
      else
      {
        context.DoPlayback(player);
        return 0;
      }
    }

    for (StringArray::const_iterator it = filesToPlay.begin(), lim = filesToPlay.end(); it != lim; ++it)
    {
      ModulePlayer::Ptr player(ModulePlayer::Create(*it, *source, context.CapFilter));
      if (!player.get() || context.DoPlayback(player))
      {
        continue;
      }
      break;
    }
    return 0;
  }
  catch (const Error& e)
  {
    std::cout << e.Text << std::endl;
    return static_cast<int>(e.Code);
  }
}
