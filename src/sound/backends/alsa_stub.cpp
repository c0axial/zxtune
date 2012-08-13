/*
Abstract:
  Alsa backend stub implementation

Last changed:
  $Id: alsa_backend.cpp 1837 2012-07-01 13:42:01Z vitamin.caig $

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "alsa.h"
#include "enumerator.h"
//library includes
#include <sound/backend_attrs.h>
//text includes
#include <sound/text/backends.h>

namespace ZXTune
{
  namespace Sound
  {
    void RegisterAlsaBackend(BackendsEnumerator& enumerator)
    {
      enumerator.RegisterCreator(CreateDisabledBackendStub(Text::ALSA_BACKEND_ID, Text::ALSA_BACKEND_DESCRIPTION, CAP_TYPE_SYSTEM));
    }

    namespace Alsa
    {
      Device::Iterator::Ptr EnumerateDevices()
      {
        return Device::Iterator::CreateStub();
      }
    }
  }
}