/*
Abstract:
  DirectSound backend stub implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#include "dsound.h"
#include "enumerator.h"
//library includes
#include <sound/backend_attrs.h>
//text includes
#include <sound/text/backends.h>

namespace ZXTune
{
  namespace Sound
  {
    void RegisterDirectSoundBackend(BackendsEnumerator& enumerator)
    {
      enumerator.RegisterCreator(CreateDisabledBackendStub(Text::DSOUND_BACKEND_ID, Text::DSOUND_BACKEND_DESCRIPTION, CAP_TYPE_SYSTEM));
    }

    namespace DirectSound
    {
      Device::Iterator::Ptr EnumerateDevices()
      {
        return Device::Iterator::CreateStub();
      }
    }
  }
}