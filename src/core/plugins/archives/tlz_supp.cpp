/*
Abstract:
  TLZ convertors support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "archive_supp_common.h"
#include <core/plugins/registrator.h>
//library includes
#include <core/plugin_attrs.h>
#include <formats/packed_decoders.h>

namespace
{
  using namespace ZXTune;

  const uint_t CAPS = CAP_STOR_CONTAINER;
}

namespace TLZ
{
  const Char ID[] = {'T', 'L', 'Z', '\0'};

  void RegisterConverter(PluginsRegistrator& registrator)
  {
    const Formats::Packed::Decoder::Ptr decoder = Formats::Packed::CreateTurboLZDecoder();
    const ArchivePlugin::Ptr plugin = CreateArchivePlugin(ID, CAPS, decoder);
    registrator.RegisterPlugin(plugin);
  }
}

namespace TLZP
{
  const Char ID[] = {'T', 'L', 'Z', 'P', '\0'};

  void RegisterConverter(PluginsRegistrator& registrator)
  {
    const Formats::Packed::Decoder::Ptr decoder = Formats::Packed::CreateTurboLZProtectedDecoder();
    const ArchivePlugin::Ptr plugin = CreateArchivePlugin(ID, CAPS, decoder);
    registrator.RegisterPlugin(plugin);
  }
}


namespace ZXTune
{
  void RegisterTLZConvertor(PluginsRegistrator& registrator)
  {
    TLZ::RegisterConverter(registrator);
    TLZP::RegisterConverter(registrator);
  }
}