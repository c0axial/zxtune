/*
Abstract:
  Null backend implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "backend_impl.h"
#include "storage.h"
//library includes
#include <l10n/api.h>
#include <sound/backend_attrs.h>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include "text/backends.h"

#define FILE_TAG 9A6FD87F

namespace
{
  const L10n::TranslateFunctor translate = L10n::TranslateFunctor("sound_backends");
}

namespace Sound
{
namespace Null
{
  const String ID = Text::NULL_BACKEND_ID;
  const char* const DESCRIPTION = L10n::translate("Null output backend");

  class BackendWorker : public Sound::BackendWorker
  {
  public:
    virtual void Test()
    {
    }

    virtual void Startup()
    {
    }

    virtual void Shutdown()
    {
    }

    virtual void Pause()
    {
    }

    virtual void Resume()
    {
    }

    virtual void BufferReady(Chunk::Ptr /*buffer*/)
    {
    }

    VolumeControl::Ptr GetVolumeControl() const
    {
      return VolumeControl::Ptr();
    }
  };

  class BackendWorkerFactory : public Sound::BackendWorkerFactory
  {
  public:
    virtual BackendWorker::Ptr CreateWorker(Parameters::Accessor::Ptr /*params*/) const
    {
      return boost::make_shared<BackendWorker>();
    }
  };
}//Null
}//Sound

namespace Sound
{
  void RegisterNullBackend(BackendsStorage& storage)
  {
    const BackendWorkerFactory::Ptr factory = boost::make_shared<Null::BackendWorkerFactory>();
    storage.Register(Null::ID, Null::DESCRIPTION, CAP_TYPE_STUB, factory);
  }
}
