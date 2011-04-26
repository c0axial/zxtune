/*
Abstract:
  CLI application implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune123 application based on zxtune library
*/

//local includes
#include "console.h"
#include "display.h"
#include "information.h"
#include "sound.h"
#include "source.h"
#include <apps/base/app.h>
#include <apps/base/error_codes.h>
#include <apps/base/parsing.h>
//common includes
#include <error_tools.h>
#include <template_parameters.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin.h>
#include <core/plugin_attrs.h>
#include <io/fs_tools.h>
#include <io/providers_parameters.h>
#include <sound/sound_parameters.h>
//std includes
#include <algorithm>
#include <cctype>
#include <functional>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
//boost includes
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
//text includes
#include "text/text.h"

#define FILE_TAG 81C76E7D

namespace
{
  inline void ErrOuter(uint_t /*level*/, Error::LocationRef loc, Error::CodeType code, const String& text)
  {
    StdOut << Error::AttributesToString(loc, code, text);
  }

  String GetModuleId(const ZXTune::Module::Information& info)
  {
    const Parameters::Accessor::Ptr accessor(info.Properties());
    String res;
    accessor->FindStringValue(ZXTune::Module::ATTR_FULLPATH, res);
    return res;
  }

  class ModuleFieldsSource : public Parameters::FieldsSourceAdapter<SkipFieldsSource>
  {
  public:
    typedef Parameters::FieldsSourceAdapter<SkipFieldsSource> Parent;
    explicit ModuleFieldsSource(const Parameters::Accessor& params)
      : Parent(params)
    {
    }

    String GetFieldValue(const String& fieldName) const
    {
      return ZXTune::IO::MakePathFromString(Parent::GetFieldValue(fieldName), '_');
    }
  };

  class Convertor
  {
  public:
    Convertor(const Parameters::Accessor& params, DisplayComponent& display)
      : Display(display)
    {
      Parameters::StringType mode;
      if (!params.FindStringValue(Text::CONVERSION_PARAM_MODE, mode))
      {
        throw Error(THIS_LINE, CONVERT_PARAMETERS, Text::CONVERT_ERROR_NO_MODE);
      }
      String nameTemplate;
      if (!params.FindStringValue(Text::CONVERSION_PARAM_FILENAME, nameTemplate))
      {
        throw Error(THIS_LINE, CONVERT_PARAMETERS, Text::CONVERT_ERROR_NO_FILENAME);
      }
      if (mode == Text::CONVERSION_MODE_RAW)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::RawConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_RAW;
      }
      else if (mode == Text::CONVERSION_MODE_PSG)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::PSGConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_PSG;
      }
      else if (mode == Text::CONVERSION_MODE_ZX50)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::ZX50ConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_ZX50;
      }
      else if (mode == Text::CONVERSION_MODE_TXT)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::TXTConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_TXT;
      }
      else if (mode == Text::CONVERSION_MODE_DEBUGAY)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::DebugAYConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_AYDUMP;
      }
      else if (mode == Text::CONVERSION_MODE_AYDUMP)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::AYDumpConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_AYDUMP;
      }
      else if (mode == Text::CONVERSION_MODE_FYM)
      {
        ConversionParameter.reset(new ZXTune::Module::Conversion::FYMConvertParam());
        CapabilityMask = ZXTune::CAP_CONV_FYM;
      }
      else
      {
        throw Error(THIS_LINE, CONVERT_PARAMETERS, Text::CONVERT_ERROR_INVALID_MODE);
      }
      FileNameTemplate = StringTemplate::Create(nameTemplate);
    }

    bool ProcessItem(ZXTune::Module::Holder::Ptr holder) const
    {
      const ZXTune::Module::Information::Ptr info = holder->GetModuleInformation();
      const String id = GetModuleId(*info);
      {
        const ZXTune::Plugin::Ptr plugin = holder->GetPlugin();
        if (!(plugin->Capabilities() & CapabilityMask))
        {
          Display.Message((Formatter(Text::CONVERT_SKIPPED) %
            id % plugin->Id()).str());
          return true;
        }
      }
      Dump result;
      ThrowIfError(holder->Convert(*ConversionParameter, result));
      //prepare result filename
      const String& filename = FileNameTemplate->Instantiate(ModuleFieldsSource(*info->Properties()));
      std::ofstream file(filename.c_str(), std::ios::binary);
      file.write(safe_ptr_cast<const char*>(&result[0]), static_cast<std::streamsize>(result.size() * sizeof(result.front())));
      if (!file)
      {
        throw MakeFormattedError(THIS_LINE, CONVERT_PARAMETERS,
          Text::CONVERT_ERROR_WRITE_FILE, filename);
      }
      Display.Message((Formatter(Text::CONVERT_DONE) % id % filename).str());
      return true;
    }
  private:
    DisplayComponent& Display;
    std::auto_ptr<ZXTune::Module::Conversion::Parameter> ConversionParameter;
    uint_t CapabilityMask;
    StringTemplate::Ptr FileNameTemplate;
  };

  class CLIApplication : public Application
  {
  public:
    CLIApplication()
      : ConfigParams(Parameters::Container::Create())
      , Informer(InformationComponent::Create())
      , Sourcer(SourceComponent::Create(ConfigParams))
      , Sounder(SoundComponent::Create(ConfigParams))
      , Display(DisplayComponent::Create())
      , SeekStep(10)
    {
    }

    virtual int Run(int argc, char* argv[])
    {
      try
      {
        if (ProcessOptions(argc, argv) ||
            Informer->Process())
        {
          return 0;
        }

        Sourcer->Initialize();

        if (!ConvertParams.empty())
        {
          const Parameters::Container::Ptr cnvParams = Parameters::Container::Create();
          ThrowIfError(ParseParametersString(String(), ConvertParams, *cnvParams));
          Convertor cnv(*cnvParams, *Display);
          Sourcer->ProcessItems(boost::bind(&Convertor::ProcessItem, &cnv, _1));
        }
        else
        {
          Sounder->Initialize();
          Sourcer->ProcessItems(boost::bind(&CLIApplication::PlayItem, this, _1));
        }
      }
      catch (const Error& e)
      {
        if (!e.FindSuberror(ZXTune::Module::ERROR_DETECT_CANCELED))
        {
          e.WalkSuberrors(ErrOuter);
        }
        return -1;
      }
      return 0;
    }
  private:
    bool ProcessOptions(int argc, char* argv[])
    {
      try
      {
        using namespace boost::program_options;

        String configFile;
        String providersOptions, coreOptions;
        options_description options((Formatter(Text::USAGE_SECTION) % *argv).str());
        options.add_options()
          (Text::HELP_KEY, Text::HELP_DESC)
          (Text::VERSION_KEY, Text::VERSION_DESC)
          (Text::CONFIG_KEY, boost::program_options::value<String>(&configFile), Text::CONFIG_DESC)
          (Text::CONVERT_KEY, boost::program_options::value<String>(&ConvertParams), Text::CONVERT_DESC)
        ;

        options.add(Informer->GetOptionsDescription());
        options.add(Sourcer->GetOptionsDescription());
        options.add(Sounder->GetOptionsDescription());
        options.add(Display->GetOptionsDescription());
        //add positional parameters for input
        positional_options_description inputPositional;
        inputPositional.add(Text::INPUT_FILE_KEY, -1);

        //cli options
        options_description cliOptions(Text::CLI_SECTION);
        cliOptions.add_options()
          (Text::SEEKSTEP_KEY, value<uint_t>(&SeekStep), Text::SEEKSTEP_DESC)
        ;
        options.add(cliOptions);

        variables_map vars;
        store(command_line_parser(argc, argv).options(options).positional(inputPositional).run(), vars);
        notify(vars);
        if (vars.count(Text::HELP_KEY))
        {
          StdOut << options << std::endl;
          return true;
        }
        else if (vars.count(Text::VERSION_KEY))
        {
          StdOut << GetProgramVersionString() << std::endl;
          return true;
        }
        ThrowIfError(ParseConfigFile(configFile, *ConfigParams));
        Sourcer->ParseParameters();
        Sounder->ParseParameters();
        return false;
      }
      catch (const std::exception& e)
      {
        throw MakeFormattedError(THIS_LINE, UNKNOWN_ERROR, Text::COMMON_ERROR, e.what());
      }
    }

    bool PlayItem(ZXTune::Module::Holder::Ptr holder)
    {
      try
      {
        const ZXTune::Sound::Backend::Ptr backend = Sounder->CreateBackend(holder);

        const uint_t frameDuration = Sounder->GetFrameDuration();

        const ZXTune::Module::Information::Ptr info = holder->GetModuleInformation();
        const uint_t seekStepFrames(info->FramesCount() * SeekStep / 100);
        ThrowIfError(backend->Play());

        Display->SetModule(backend->GetPlayer(), static_cast<uint_t>(frameDuration));

        ZXTune::Sound::Gain curVolume = ZXTune::Sound::Gain();
        ZXTune::Sound::MultiGain allVolume;
        ZXTune::Sound::VolumeControl::Ptr volCtrl(backend->GetVolumeControl());
        const bool noVolume = volCtrl.get() == 0;
        if (!noVolume)
        {
          ThrowIfError(volCtrl->GetVolume(allVolume));
          curVolume = std::accumulate(allVolume.begin(), allVolume.end(), curVolume) / allVolume.size();
        }

        ZXTune::Sound::Backend::State state = ZXTune::Sound::Backend::FAILED;
        Error stateError;

        for (;;)
        {
          state = backend->GetCurrentState(&stateError);

          if (ZXTune::Sound::Backend::FAILED == state)
          {
            throw stateError;
          }

          const uint_t curFrame = Display->BeginFrame(state);

          if (const uint_t key = Console::Self().GetPressedKey())
          {
            switch (key)
            {
            case Console::INPUT_KEY_CANCEL:
            case 'Q':
              return false;
            case Console::INPUT_KEY_LEFT:
              ThrowIfError(backend->SetPosition(curFrame < seekStepFrames ? 0 : curFrame - seekStepFrames));
              break;
            case Console::INPUT_KEY_RIGHT:
              ThrowIfError(backend->SetPosition(curFrame + seekStepFrames));
              break;
            case Console::INPUT_KEY_DOWN:
              if (!noVolume)
              {
                curVolume = std::max(0.0, curVolume - 0.05);
                ZXTune::Sound::MultiGain allVol;
                allVol.assign(curVolume);
                ThrowIfError(volCtrl->SetVolume(allVol));
              }
              break;
            case Console::INPUT_KEY_UP:
              if (!noVolume)
              {
                curVolume = std::min(1.0, curVolume + 0.05);
                ZXTune::Sound::MultiGain allVol;
                allVol.assign(curVolume);
                ThrowIfError(volCtrl->SetVolume(allVol));
              }
              break;
            case Console::INPUT_KEY_ENTER:
              if (ZXTune::Sound::Backend::STARTED == state)
              {
                ThrowIfError(backend->Pause());
                Console::Self().WaitForKeyRelease();
              }
              else
              {
                Console::Self().WaitForKeyRelease();
                ThrowIfError(backend->Play());
              }
              break;
            case ' ':
              ThrowIfError(backend->Stop());
              state = ZXTune::Sound::Backend::STOPPED;
              Console::Self().WaitForKeyRelease();
              break;
            }
          }

          if (ZXTune::Sound::Backend::STOPPED == state)
          {
            break;
          }
          Display->EndFrame();
        }
      }
      catch (const Error& e)
      {
        e.WalkSuberrors(ErrOuter);
      }
      return true;
    }
  private:
    const Parameters::Container::Ptr ConfigParams;
    String ConvertParams;
    std::auto_ptr<InformationComponent> Informer;
    std::auto_ptr<SourceComponent> Sourcer;
    std::auto_ptr<SoundComponent> Sounder;
    std::auto_ptr<DisplayComponent> Display;
    uint_t SeekStep;
  };
}

const std::string THIS_MODULE("zxtune123");

std::auto_ptr<Application> Application::Create()
{
  return std::auto_ptr<Application>(new CLIApplication());
}
