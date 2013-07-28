/*
Abstract:
  TFM-based track players common functionality implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "tfm_base_track.h"
//boost includes
#include <boost/make_shared.hpp>

namespace
{
  inline uint_t EncodeDetune(int_t in)
  {
    if (in >= 0)
    {
      return in;
    }
    else
    {
      return 4 - in;
    }
  }
}

namespace Module
{
  namespace TFM
  {
    class TrackDataIterator : public DataIterator
    {
    public:
      TrackDataIterator(TrackStateIterator::Ptr delegate, DataRenderer::Ptr renderer)
        : Delegate(delegate)
        , State(Delegate->GetStateObserver())
        , Render(renderer)
      {
        FillCurrentData();
      }

      virtual void Reset()
      {
        Delegate->Reset();
        Render->Reset();
        FillCurrentData();
      }

      virtual bool IsValid() const
      {
        return Delegate->IsValid();
      }

      virtual void NextFrame(bool looped)
      {
        Delegate->NextFrame(looped);
        FillCurrentData();
      }

      virtual TrackState::Ptr GetStateObserver() const
      {
        return State;
      }

      virtual Devices::TFM::Registers GetData() const
      {
        return CurrentData;
      }
    private:
      void FillCurrentData()
      {
        if (Delegate->IsValid())
        {
          TrackBuilder builder;
          Render->SynthesizeData(*State, builder);
          CurrentData = builder.GetResult();
        }
      }
    private:
      const TrackStateIterator::Ptr Delegate;
      const TrackModelState::Ptr State;
      const DataRenderer::Ptr Render;
      Devices::TFM::Registers CurrentData;
    };

    ChannelBuilder::ChannelBuilder(uint_t chan, Devices::TFM::Registers& data)
      : Channel(chan >= TFM::TRACK_CHANNELS / 2 ? chan - TFM::TRACK_CHANNELS / 2 : chan)
      , Registers(data[chan >= TFM::TRACK_CHANNELS / 2])
    {
    }

    void ChannelBuilder::SetMode(uint_t mode)
    {
      WriteChipRegister(0x27, mode);
    }

    void ChannelBuilder::KeyOn()
    {
      SetKey(0xf);
    }

    void ChannelBuilder::KeyOff()
    {
      SetKey(0);
    }

    void ChannelBuilder::SetKey(uint_t mask)
    {
      WriteChipRegister(0x28, Channel | (mask << 4));
    }

    void ChannelBuilder::SetupConnection(uint_t algorithm, uint_t feedback)
    {
      const uint_t val = algorithm | (feedback << 3);
      WriteChannelRegister(0xb0, val);
    }

    void ChannelBuilder::SetDetuneMultiple(uint_t op, int_t detune, uint_t multiple)
    {
      const uint_t val = (EncodeDetune(detune) << 4) | multiple;
      WriteOperatorRegister(0x30, op, val);
    }

    void ChannelBuilder::SetRateScalingAttackRate(uint_t op, uint_t rateScaling, uint_t attackRate)
    {
      const uint_t val = (rateScaling << 6) | attackRate;
      WriteOperatorRegister(0x50, op, val);
    }

    void ChannelBuilder::SetDecay(uint_t op, uint_t decay)
    {
      WriteOperatorRegister(0x60, op, decay);
    }

    void ChannelBuilder::SetSustain(uint_t op, uint_t sustain)
    {
      WriteOperatorRegister(0x70, op, sustain);
    }

    void ChannelBuilder::SetSustainLevelReleaseRate(uint_t op, uint_t sustainLevel, uint_t releaseRate)
    {
      const uint_t val = (sustainLevel << 4) | releaseRate;
      WriteOperatorRegister(0x80, op, val);
    }

    void ChannelBuilder::SetEnvelopeType(uint_t op, uint_t type)
    {
      WriteOperatorRegister(0x90, op, type);
    }

    void ChannelBuilder::SetTotalLevel(uint_t op, uint_t totalLevel)
    {
      WriteOperatorRegister(0x40, op, totalLevel);
    }

    void ChannelBuilder::SetTone(uint_t octave, uint_t tone)
    {
      const uint_t valHi = octave * 8 + (tone >> 8);
      const uint_t valLo = tone & 0xff;
      WriteChannelRegister(0xa4, valHi);
      WriteChannelRegister(0xa0, valLo);
    }

    void ChannelBuilder::SetTone(uint_t op, uint_t octave, uint_t tone)
    {
      const uint_t valHi = octave * 8 + (tone >> 8);
      const uint_t valLo = tone & 0xff;
      assert(Channel == 2);
      switch (op)
      {
      case 0:
        WriteChipRegister(0xa4, valHi);
        WriteChipRegister(0xa0, valLo);
        break;
      case 1:
        WriteChipRegister(0xac, valHi);
        WriteChipRegister(0xa8, valLo);
        break;
      case 2:
        WriteChipRegister(0xae, valHi);
        WriteChipRegister(0xaa, valLo);
        break;
      case 3:
        //op1 in doc???
        WriteChipRegister(0xad, valHi);
        WriteChipRegister(0xa9, valLo);
        break;
      }
    }

    void ChannelBuilder::SetPane(uint_t val)
    {
      WriteChannelRegister(0xb4, val);
    }

    void ChannelBuilder::WriteOperatorRegister(uint_t base, uint_t op, uint_t val)
    {
      WriteChannelRegister(base + 4 * op, val);
    }

    void ChannelBuilder::WriteChannelRegister(uint_t base, uint_t val)
    {
      Registers.push_back(Devices::FM::Register(base + Channel, val));
    }

    void ChannelBuilder::WriteChipRegister(uint_t idx, uint_t val)
    {
      const Devices::FM::Register reg(idx, val);
      Registers.push_back(reg);
    }

    DataIterator::Ptr CreateDataIterator(TrackStateIterator::Ptr iterator, DataRenderer::Ptr renderer)
    {
      return boost::make_shared<TrackDataIterator>(iterator, renderer);
    }
  }
}