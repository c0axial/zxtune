/*
Abstract:
  ST1 modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_conversion.h"
#include "soundtracker.h"
#include "core/plugins/utils.h"
#include "core/plugins/registrator.h"
#include "core/plugins/players/creation_result.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/error_codes.h>
#include <core/plugin_attrs.h>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include <core/text/plugins.h>

namespace ST1
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct Sample
  {
    uint8_t Volume[SoundTracker::SAMPLE_ORNAMENT_SIZE];
    uint8_t Noise[SoundTracker::SAMPLE_ORNAMENT_SIZE];
    uint16_t Effect[SoundTracker::SAMPLE_ORNAMENT_SIZE];
    uint8_t Loop;
    uint8_t LoopSize;
  } PACK_POST;

  PACK_PRE struct PosEntry
  {
    uint8_t Pattern;
    int8_t Transposition;
  } PACK_POST;

  PACK_PRE struct Ornament
  {
    int8_t Offsets[SoundTracker::SAMPLE_ORNAMENT_SIZE];
  } PACK_POST;

  PACK_PRE struct Pattern
  {
    PACK_PRE struct Line
    {
      PACK_PRE struct Channel
      {
        //RNNN#OOO
        uint8_t Note;
        //SSSSEEEE
        uint8_t EffectSample;
        //EEEEeeee
        uint8_t EffectOrnament;

        bool IsEmpty() const
        {
          return !IsRest() && !HasNote() && 0 == EffectSample;
        }

        bool IsRest() const
        {
          return 0 != (Note & 128);
        }

        bool HasNote() const
        {
          return 0 != (Note & 0x78);
        }

        uint_t GetEffect() const
        {
          return EffectSample & 15;
        }

        uint_t GetEffectParam() const
        {
          return EffectOrnament;
        }

        uint_t GetSample() const
        {
          return EffectSample >> 4;
        }

        uint_t GetOrnament() const
        {
          return EffectOrnament & 15;
        }
      } PACK_POST;

      Channel Channels[3];

      bool IsEmpty() const
      {
        return Channels[0].IsEmpty() && Channels[1].IsEmpty() && Channels[2].IsEmpty();
      }
    } PACK_POST;

    Line Lines[SoundTracker::MAX_PATTERN_SIZE];
  } PACK_POST;

  PACK_PRE struct Header
  {
    Sample Samples[15];
    PosEntry Positions[256];
    uint8_t Lenght;
    Ornament Ornaments[17];
    uint8_t Tempo;
    uint8_t PatternsSize;
    Pattern Patterns[1];
  } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(Sample) == 130);
  BOOST_STATIC_ASSERT(sizeof(PosEntry) == 2);
  BOOST_STATIC_ASSERT(sizeof(Ornament) == 32);
  BOOST_STATIC_ASSERT(sizeof(Pattern) == 576);
  BOOST_STATIC_ASSERT(sizeof(Header) == 3009 + 576);
  BOOST_STATIC_ASSERT(offsetof(Header, Positions) == 1950);
  BOOST_STATIC_ASSERT(offsetof(Header, Lenght) == 2462);
  BOOST_STATIC_ASSERT(offsetof(Header, Tempo) == 3007);
  BOOST_STATIC_ASSERT(offsetof(Header, Patterns) == 3009);

  class ModuleData : public SoundTracker::ModuleData
  {
  public:
    typedef boost::shared_ptr<ModuleData> Ptr;

    void ParseInformation(const Header& header, ModuleProperties& properties)
    {
      InitialTempo = header.Tempo;
      properties.SetProgram(Text::ST_EDITOR);
      properties.SetFreqtable(TABLE_SOUNDTRACKER);
    }

    void ParseSamples(const Header& header)
    {
      Samples.resize(1);
      std::for_each(header.Samples, ArrayEnd(header.Samples), boost::bind(&ModuleData::AddSample, this, _1));
    }

    void ParseOrnaments(const Header& header)
    {
      Ornaments.clear();
      std::for_each(header.Ornaments, ArrayEnd(header.Ornaments), boost::bind(&ModuleData::AddOrnament, this, _1));
    }

    std::size_t ParsePositionsAndPatterns(const Header& header, std::size_t size)
    {
      const uint_t patternsCount = ParsePositions(header);
      const uint_t realPatternsCount = std::min<uint_t>(1 + (size - sizeof(header)) / sizeof(Pattern), SoundTracker::MAX_PATTERNS_COUNT);
      const uint_t patternsToParse = std::min(patternsCount, realPatternsCount);
      ParsePatterns(header, patternsToParse);
      for (uint_t emptyPatterns = patternsToParse; emptyPatterns != patternsCount; ++emptyPatterns)
      {
        SoundTracker::Track::Pattern empty;
        empty.AddLines(header.PatternsSize);
        Patterns.push_back(empty);
      }
      return sizeof(Header) + (patternsToParse - 1) * sizeof(Pattern);
    }
  private:
    void AddSample(const Sample& sample)
    {
      std::vector<SoundTracker::Sample::Line> lines;
      for (uint_t idx = 0; idx < SoundTracker::SAMPLE_ORNAMENT_SIZE; ++idx)
      {
        SoundTracker::Sample::Line res;
        res.Level = sample.Volume[idx];
        res.Noise = sample.Noise[idx] & 31;
        res.NoiseMask = 0 != (sample.Noise[idx] & 128);
        res.EnvelopeMask = 0 != (sample.Noise[idx] & 64);
        const int16_t eff = fromLE(sample.Effect[idx]);
        res.Effect = 0 != (eff & 0x1000) ? (eff & 0xfff) : -(eff & 0xfff);
        lines.push_back(res);
      }
      const uint_t loop = std::min<uint_t>(sample.Loop, static_cast<uint_t>(lines.size()));
      const uint_t loopLimit = std::min<uint_t>(sample.Loop + sample.LoopSize + 1, static_cast<uint_t>(lines.size()));
      Samples.push_back(SoundTracker::Sample(loop, loopLimit, lines.begin(), lines.end()));
    }

    void AddOrnament(const Ornament& ornament)
    {
      Ornaments.push_back(SoundTracker::Track::Ornament(0, ornament.Offsets, ArrayEnd(ornament.Offsets)));
    }

    uint_t ParsePositions(const Header& header)
    {
      const std::size_t posCount = header.Lenght + 1;
      Positions.resize(posCount);
      std::transform(header.Positions, header.Positions + posCount, Positions.begin(), 
        boost::bind(std::minus<uint_t>(), boost::bind(&PosEntry::Pattern, _1), uint_t(1)));
      Transpositions.resize(posCount);
      std::transform(header.Positions, header.Positions + posCount, Transpositions.begin(), boost::mem_fn(&PosEntry::Transposition));
      return 1 + *std::max_element(Positions.begin(), Positions.end());
    }

    void ParsePatterns(const Header& header, uint_t count)
    {
      Patterns.clear();
      const uint_t patSize = header.PatternsSize;
      std::for_each(header.Patterns, header.Patterns + count, boost::bind(&ModuleData::AddPattern, this, _1, patSize));
    }

    struct ChanState
    {
      uint_t Sample;
      uint_t Ornament;
      uint_t EnvType;
      uint_t EnvTone;

      ChanState()
        : Sample(), Ornament()
        , EnvType(), EnvTone()
      {
      }
    };

    void AddPattern(const Pattern& pattern, uint_t patSize)
    {
      SoundTracker::Track::Pattern result;
      uint_t linesToSkip = 0;
      boost::array<ChanState, 3> state;
      for (uint_t idx = 0; idx < patSize; ++idx)
      {
        const Pattern::Line& srcLine = pattern.Lines[idx];
        if (srcLine.IsEmpty())
        {
          ++linesToSkip;
          continue;
        }
        if (linesToSkip)
        {
          result.AddLines(linesToSkip);
          linesToSkip = 0;
        }
        SoundTracker::Track::Line& dstLine = result.AddLine();
        ConvertLine(srcLine, dstLine, state);
      }
      if (linesToSkip)
      {
        result.AddLines(linesToSkip);
      }
      Patterns.push_back(result);
    }

    static void ConvertLine(const Pattern::Line& srcLine, SoundTracker::Track::Line& dstLine, boost::array<ChanState, 3>& state)
    {
      for (uint_t chan = 0; chan < dstLine.Channels.size(); ++chan)
      {
        ConvertChannel(srcLine.Channels[chan], dstLine.Channels[chan], state[chan]);
      }
    }

    static void ConvertChannel(const Pattern::Line::Channel& srcChan, SoundTracker::Track::Line::Chan& dstChan, ChanState& state)
    {
      if (srcChan.IsEmpty())
      {
        return;
      }
      if (srcChan.IsRest())
      {
        dstChan.SetEnabled(false);
        return;
      }
      else if (!srcChan.HasNote())
      {
        return;
      }
      dstChan.SetNote(ConvertNote(srcChan.Note));
      dstChan.SetEnabled(true);
      {
        const uint_t sample = srcChan.GetSample();
        if (sample && sample != state.Sample)
        {
          dstChan.SetSample(sample);
        }
        state.Sample = sample;
      }
      switch (const uint_t effect = srcChan.GetEffect())
      {
      case 15:
        {
          const uint_t ornament = srcChan.GetOrnament();
          if (ornament != state.Ornament)
          {
            dstChan.SetOrnament(ornament);
            state.Ornament = ornament;
          }
          dstChan.Commands.push_back(SoundTracker::Track::Command(SoundTracker::NOENVELOPE));
          state.EnvType = 0;
        }
        break;
      case 8:
      case 10:
      case 12:
      case 14:
        {
          const uint_t envType = effect;
          const uint_t envTone = srcChan.GetEffectParam();
          if (envType != state.EnvType || envTone != state.EnvTone)
          {
            dstChan.SetOrnament(0);
            dstChan.Commands.push_back(SoundTracker::Track::Command(SoundTracker::ENVELOPE, envType, envTone));
            state.Ornament = 0;
            state.EnvType = envType;
            state.EnvTone = envTone;
          }
        }
        break;
      default:
        state.EnvType = effect;
        dstChan.Commands.push_back(SoundTracker::Track::Command(SoundTracker::NOENVELOPE));
        break;
      }
    }

    static uint_t ConvertNote(uint8_t note)
    {
      static const uint_t HALFTONES[] = 
      {
        -1, //invalid
        -1, //invalid#
        9,  //A
        10, //A#
        11, //B
        -1, //B#,invalid
        0,  //C
        1,  //C#
        2,  //D
        3,  //D#
        4,  //E
        -1, //E#,invalid
        5,  //F
        6,  //F#
        7,  //G
        8,  //G#
      };

      const uint_t NOTES_PER_OCTAVE = 12;
      const uint_t octave = note & 7;
      const uint_t halftone = (note & 0x78) >> 3;
      return HALFTONES[halftone] + NOTES_PER_OCTAVE * octave;
    }
  };

  bool CheckModule(const void* data, std::size_t size)
  {
    if (size < sizeof(Header))
    {
      return false;
    }
    const Header& header = *static_cast<const Header*>(data);
    std::vector<uint_t> positions(header.Lenght + 1);
    std::transform(header.Positions, header.Positions + positions.size(), positions.begin(), boost::mem_fn(&PosEntry::Pattern));
    const uint_t patternsCount = *std::max_element(positions.begin(), positions.end());
    const uint_t availablePatterns = std::min<uint_t>(1 + (size - sizeof(header)) / sizeof(Pattern), SoundTracker::MAX_PATTERNS_COUNT);
    const uint_t patternsToCheck = std::min(patternsCount, availablePatterns);
    const uint_t patSize = header.PatternsSize;
    for (std::size_t idx = 0; idx < patternsToCheck; ++idx)
    {
      const Pattern& pat = header.Patterns[idx];
      const Pattern::Line* const endOfPattern = ArrayEnd(pat.Lines);
      const Pattern::Line* const lastNotEmpty = std::find_if(pat.Lines + patSize, endOfPattern, !boost::bind(&Pattern::Line::IsEmpty, _1));
      if (lastNotEmpty != endOfPattern)
      {
        return false;
      }
    }
    return true;
  }
}

namespace ST1
{
  using namespace ZXTune;

  //plugin attributes
  const Char ID[] = {'S', 'T', '1', 0};
  const Char* const INFO = Text::ST1_PLUGIN_INFO;
  const String VERSION(FromStdString("$Rev$"));
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | Module::GetSupportedAYMFormatConvertors();

  const std::string ST11_FORMAT(
    //samples
    "("
      //levels
      "00-0f{32}"
      //noises. Bit 5 should be empty, but due to detector performance lack keep only ranges
      "?{32}"
      //additions
      "(?00-1f){32}"
      //loop, loop limit
      "00-1f{2}"
    "){15}"
    //positions
    "(01-20?){256}"
    //length
    "00-7f"
    //ornaments
    "(?{32}){17}"
    //delay
    //Real delay may be from 01 but I don't know any module with such little delay
    "02-0f"
    //patterns size
    //Real pattern size may be from 01 but I don't know any modules with such patterns size
    "20-40"
  );

  //addon for ZXAYST11 header
  const std::size_t MAX_SEARCH_WINDOW = 48 + sizeof(ST1::Header) + (SoundTracker::MAX_PATTERNS_COUNT - 1) * sizeof(ST1::Pattern);

  class OptimizedFormat : public Binary::Format
  {
  public:
    OptimizedFormat()
      : Delegate(Binary::Format::Create(ST11_FORMAT))
    {
    }
    
    virtual bool Match(const void* data, std::size_t size) const
    {
      return Delegate->Match(data, size);
    }
    
    virtual std::size_t Search(const void* data, std::size_t size) const
    {
      return size > MAX_SEARCH_WINDOW
        ? size
        : Delegate->Search(data, size);
    }
  private:
    const Binary::Format::Ptr Delegate;
  };
  //////////////////////////////////////////////////////////////////////////
  class Factory : public ModulesFactory
  {
  public:
    Factory()
      : Format(boost::make_shared<OptimizedFormat>())
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
      const std::size_t size = inputData.Size();
      return Format->Match(data, size) && ST1::CheckModule(data, size);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Format;
    }

    virtual Module::Holder::Ptr CreateModule(Module::ModuleProperties::RWPtr properties, Parameters::Accessor::Ptr parameters, Binary::Container::Ptr allData, std::size_t& usedSize) const
    {
      try
      {
        assert(Check(*allData));

        //assume that data is ok
        const ST1::Header& header = *static_cast<const ST1::Header*>(allData->Data());

        const ST1::ModuleData::Ptr parsedData = boost::make_shared<ST1::ModuleData>();
        parsedData->ParseInformation(header, *properties);
        parsedData->ParseSamples(header);
        parsedData->ParseOrnaments(header);
        usedSize = parsedData->ParsePositionsAndPatterns(header, allData->Size());
        //meta properties
        {
          const std::size_t fixedOffset = sizeof(ST1::Header) - sizeof(ST1::Pattern);
          const ModuleRegion fixedRegion(fixedOffset, usedSize - fixedOffset);
          properties->SetSource(usedSize, fixedRegion);
        }
        const Module::AYM::Chiptune::Ptr chiptune = SoundTracker::CreateChiptune(parsedData, properties);
        return Module::AYM::CreateHolder(chiptune, parameters);
      }
      catch (const Error& /*e*/)
      {
        Log::Debug("Core::ST11Supp", "Failed to create holder");
      }
      return Module::Holder::Ptr();
    }
  private:
    const Binary::Format::Ptr Format;
  };
}

namespace ZXTune
{
  void RegisterST1Support(PluginsRegistrator& registrator)
  {
    const ModulesFactory::Ptr factory = boost::make_shared<ST1::Factory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ST1::ID, ST1::INFO, ST1::VERSION, ST1::CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}
