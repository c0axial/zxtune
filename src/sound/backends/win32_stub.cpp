/*
Abstract:
  Win32 waveout backend stub implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#include "win32.h"
#include "enumerator.h"
//library includes
#include <sound/backend_attrs.h>
//text includes
#include <sound/text/backends.h>

namespace ZXTune
{
  namespace Sound
  {
    void RegisterWin32Backend(BackendsEnumerator& enumerator)
    {
      enumerator.RegisterCreator(CreateDisabledBackendStub(Text::WIN32_BACKEND_ID, Text::WIN32_BACKEND_DESCRIPTION, CAP_TYPE_SYSTEM));
    }

    namespace Win32
    {
      Device::Iterator::Ptr EnumerateDevices()
      {
        return Device::Iterator::CreateStub();
      }
    }
  }
}