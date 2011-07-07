/*
Abstract:
  OSS backend implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifdef OSS_SUPPORT

//local includes
#include "backend_impl.h"
#include "backend_wrapper.h"
#include "enumerator.h"
//common includes
#include <tools.h>
#include <error_tools.h>
#include <logging.h>
//library includes
#include <io/fs_tools.h>
#include <sound/backend_attrs.h>
#include <sound/backends_parameters.h>
#include <sound/error_codes.h>
#include <sound/sound_parameters.h>
//platform-specific includes
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
//std includes
#include <algorithm>
//boost includes
#include <boost/noncopyable.hpp>
//text includes
#include <sound/text/backends.h>
#include <sound/text/sound.h>

#define FILE_TAG 69200152

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Sound;

  const std::string THIS_MODULE("Sound::Backend::OSS");

  const uint_t MAX_OSS_VOLUME = 100;

  const Char OSS_BACKEND_ID[] = {'o', 's', 's', 0};
  const String OSS_BACKEND_VERSION(FromStdString("$Rev$"));

  class AutoDescriptor : public boost::noncopyable
  {
  public:
    AutoDescriptor()
      : Handle(-1)
    {
    }
    AutoDescriptor(const String& name, int mode)
      : Name(name)
      , Handle(::open(IO::ConvertToFilename(name).c_str(), mode, 0))
    {
      CheckResult(-1 != Handle, THIS_LINE);
      Log::Debug(THIS_MODULE, "Opened device '%1%'", Name);
    }

    ~AutoDescriptor()
    {
      try
      {
        Close();
      }
      catch (const Error&)
      {
      }
    }

    void CheckResult(bool res, Error::LocationRef loc) const
    {
      if (!res)
      {
        throw MakeFormattedError(loc, BACKEND_PLATFORM_ERROR,
          Text::SOUND_ERROR_OSS_BACKEND_ERROR, Name, ::strerror(errno));
      }
    }

    void Swap(AutoDescriptor& rh)
    {
      std::swap(rh.Handle, Handle);
      std::swap(rh.Name, Name);
    }

    void Close()
    {
      if (-1 != Handle)
      {
        Log::Debug(THIS_MODULE, "Close device '%1%'", Name);
        int tmpHandle = -1;
        std::swap(Handle, tmpHandle);
        Name.clear();
        CheckResult(0 == ::close(tmpHandle), THIS_LINE);
      }
    }

    int Get() const
    {
      return Handle;
    }
  private:
    String Name;
    //leave handle as int
    int Handle;
  };

  class OSSVolumeControl : public VolumeControl
  {
  public:
    OSSVolumeControl(boost::mutex& stateMutex, AutoDescriptor& mixer)
      : StateMutex(stateMutex), MixHandle(mixer)
    {
    }

    virtual Error GetVolume(MultiGain& volume) const
    {
      try
      {
        Log::Debug(THIS_MODULE, "GetVolume");
        boost::mutex::scoped_lock lock(StateMutex);
        if (-1 != MixHandle.Get())
        {
          boost::array<uint8_t, sizeof(int)> buf;
          MixHandle.CheckResult(-1 != ::ioctl(MixHandle.Get(), SOUND_MIXER_READ_VOLUME,
            safe_ptr_cast<int*>(&buf[0])), THIS_LINE);
          std::transform(buf.begin(), buf.begin() + OUTPUT_CHANNELS, volume.begin(),
            std::bind2nd(std::divides<Gain>(), MAX_OSS_VOLUME));
        }
        else
        {
          volume = MultiGain();
        }
        return Error();
      }
      catch (const Error& e)
      {
        return e;
      }
    }

    virtual Error SetVolume(const MultiGain& volume)
    {
      if (volume.end() != std::find_if(volume.begin(), volume.end(), std::bind2nd(std::greater<Gain>(), Gain(1.0))))
      {
        return Error(THIS_LINE, BACKEND_INVALID_PARAMETER, Text::SOUND_ERROR_BACKEND_INVALID_GAIN);
      }
      try
      {
        Log::Debug(THIS_MODULE, "SetVolume");
        boost::mutex::scoped_lock lock(StateMutex);
        if (-1 != MixHandle.Get())
        {
          boost::array<uint8_t, sizeof(int)> buf = { {0} };
          std::transform(volume.begin(), volume.end(), buf.begin(),
            std::bind2nd(std::multiplies<Gain>(), MAX_OSS_VOLUME));
          MixHandle.CheckResult(-1 != ::ioctl(MixHandle.Get(), SOUND_MIXER_WRITE_VOLUME,
            safe_ptr_cast<int*>(&buf[0])), THIS_LINE);
        }
        return Error();
      }
      catch (const Error& e)
      {
        return e;
      }
    }
  private:
    boost::mutex& StateMutex;
    AutoDescriptor& MixHandle;
  };

  class OSSBackendParameters
  {
  public:
    explicit OSSBackendParameters(const Parameters::Accessor& accessor)
      : Accessor(accessor)
    {
    }

    String GetDeviceName() const
    {
      Parameters::StringType strVal = Parameters::ZXTune::Sound::Backends::OSS::DEVICE_DEFAULT;
      Accessor.FindStringValue(Parameters::ZXTune::Sound::Backends::OSS::DEVICE, strVal);
      return strVal;
    }

    String GetMixerName() const
    {
      Parameters::StringType strVal = Parameters::ZXTune::Sound::Backends::OSS::MIXER_DEFAULT;
      Accessor.FindStringValue(Parameters::ZXTune::Sound::Backends::OSS::MIXER, strVal);
      return strVal;
    }
  private:
    const Parameters::Accessor& Accessor;
  };

  class OSSBackendWorker : public BackendWorker
                         , private boost::noncopyable
  {
  public:
    explicit OSSBackendWorker(Parameters::Accessor::Ptr params)
      : BackendParams(params)
      , RenderingParameters(RenderParameters::Create(BackendParams))
      , CurrentBuffer(Buffers.begin(), Buffers.end())
      , VolumeController(new OSSVolumeControl(StateMutex, MixHandle))
    {
    }

    virtual ~OSSBackendWorker()
    {
      assert(-1 == DevHandle.Get() || !"OSSBackend should be stopped before destruction.");
    }

    virtual void Test()
    {
      OnStartup();
      OnShutdown();
    }

    virtual VolumeControl::Ptr GetVolumeControl() const
    {
      return VolumeController;
    }

    virtual void OnStartup()
    {
      assert(-1 == MixHandle.Get() && -1 == DevHandle.Get());

      const OSSBackendParameters params(*BackendParams);

      AutoDescriptor tmpMixer(params.GetMixerName(), O_RDWR);
      AutoDescriptor tmpDevice(params.GetDeviceName(), O_WRONLY);

      BOOST_STATIC_ASSERT(1 == sizeof(Sample) || 2 == sizeof(Sample));
      int tmp(2 == sizeof(Sample) ? AFMT_S16_NE : AFMT_S8);
      Log::Debug(THIS_MODULE, "Setting format to %1%", tmp);
      tmpDevice.CheckResult(-1 != ::ioctl(tmpDevice.Get(), SNDCTL_DSP_SETFMT, &tmp), THIS_LINE);

      tmp = OUTPUT_CHANNELS;
      Log::Debug(THIS_MODULE, "Setting channels to %1%", tmp);
      tmpDevice.CheckResult(-1 != ::ioctl(tmpDevice.Get(), SNDCTL_DSP_CHANNELS, &tmp), THIS_LINE);

      tmp = RenderingParameters->SoundFreq();
      Log::Debug(THIS_MODULE, "Setting frequency to %1%", tmp);
      tmpDevice.CheckResult(-1 != ::ioctl(tmpDevice.Get(), SNDCTL_DSP_SPEED, &tmp), THIS_LINE);

      DevHandle.Swap(tmpDevice);
      MixHandle.Swap(tmpMixer);
      Log::Debug(THIS_MODULE, "Successfully opened");
    }

    virtual void OnShutdown()
    {
      DevHandle.Close();
      MixHandle.Close();
      Log::Debug(THIS_MODULE, "Successfully closed");
    }

    virtual void OnPause()
    {
    }

    virtual void OnResume()
    {
    }

    virtual void OnFrame()
    {
    }

    virtual void OnBufferReady(std::vector<MultiSample>& buffer)
    {
      std::vector<MultiSample>& buf(*CurrentBuffer);
      buf.swap(buffer);
      assert(-1 != DevHandle.Get());
      std::size_t toWrite(buf.size() * sizeof(buf.front()));
      const uint8_t* data(safe_ptr_cast<const uint8_t*>(&buf[0]));
      while (toWrite)
      {
        const int res = ::write(DevHandle.Get(), data, toWrite * sizeof(*data));
        DevHandle.CheckResult(res >= 0, THIS_LINE);
        toWrite -= res;
        data += res;
      }
      ++CurrentBuffer;
    }
  private:
    const Parameters::Accessor::Ptr BackendParams;
    const RenderParameters::Ptr RenderingParameters;
    boost::mutex StateMutex;
    AutoDescriptor MixHandle;
    AutoDescriptor DevHandle;
    boost::array<std::vector<MultiSample>, 2> Buffers;
    CycledIterator<std::vector<MultiSample>*> CurrentBuffer;
    const VolumeControl::Ptr VolumeController;
  };

  class OSSBackendCreator : public BackendCreator
  {
  public:
    virtual String Id() const
    {
      return OSS_BACKEND_ID;
    }

    virtual String Description() const
    {
      return Text::OSS_BACKEND_DESCRIPTION;
    }

    virtual String Version() const
    {
      return OSS_BACKEND_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_TYPE_SYSTEM | CAP_FEAT_HWVOLUME;
    }

    virtual Error CreateBackend(CreateBackendParameters::Ptr params, Backend::Ptr& result) const
    {
      try
      {
        const Parameters::Accessor::Ptr allParams = params->GetParameters();
        const BackendWorker::Ptr worker(new OSSBackendWorker(allParams));
        result = Sound::CreateBackend(params, worker);
        return Error();
      }
      catch (const Error& e)
      {
        return MakeFormattedError(THIS_LINE, BACKEND_FAILED_CREATE,
          Text::SOUND_ERROR_BACKEND_FAILED, Id()).AddSuberror(e);
      }
      catch (const std::bad_alloc&)
      {
        return Error(THIS_LINE, BACKEND_NO_MEMORY, Text::SOUND_ERROR_BACKEND_NO_MEMORY);
      }
    }
  };
}

namespace ZXTune
{
  namespace Sound
  {
    void RegisterOSSBackend(BackendsEnumerator& enumerator)
    {
      const BackendCreator::Ptr creator(new OSSBackendCreator());
      enumerator.RegisterCreator(creator);
    }
  }
}

#else //not supported

namespace ZXTune
{
  namespace Sound
  {
    void RegisterOSSBackend(class BackendsEnumerator& /*enumerator*/)
    {
      //do nothing
    }
  }
}

#endif
