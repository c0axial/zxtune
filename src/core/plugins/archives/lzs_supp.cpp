/*
Abstract:
  LZS convertors support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include <core/plugins/enumerator.h>
//common includes
#include <tools.h>
//library includes
#include <core/plugin_attrs.h>
#include <formats/packed_decoders.h>
#include <io/container.h>
//text includes
#include <core/text/plugins.h>

namespace
{
  using namespace ZXTune;

  const Char LZS_PLUGIN_ID[] = {'L', 'Z', 'S', '\0'};
  const String LZS_PLUGIN_VERSION(FromStdString("$Rev$"));

  class LZSPlugin : public ArchivePlugin
  {
  public:
    LZSPlugin()
      : Decoder(Formats::Packed::CreateLZSDecoder())
    {
    }

    virtual String Id() const
    {
      return LZS_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::LZS_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return LZS_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_CONTAINER;
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      return Decoder->Check(inputData.Data(), inputData.Size());
    }

    virtual IO::DataContainer::Ptr ExtractSubdata(const Parameters::Accessor& /*commonParams*/,
      const IO::DataContainer& data, ModuleRegion& region) const
    {
      std::auto_ptr<Dump> res = Decoder->Decode(data.Data(), data.Size(), region.Size);
      if (res.get())
      {
        region.Offset = 0;
        return IO::CreateDataContainer(res);
      }
      return IO::DataContainer::Ptr();
    }
  private:
    const Formats::Packed::Decoder::Ptr Decoder;
  };
}

namespace ZXTune
{
  void RegisterLZSConvertor(PluginsEnumerator& enumerator)
  {
    const ArchivePlugin::Ptr plugin(new LZSPlugin());
    enumerator.RegisterPlugin(plugin);
  }
}
