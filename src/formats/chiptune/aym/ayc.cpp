/**
* 
* @file
*
* @brief  AYC support implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "ayc.h"
#include "formats/chiptune/container.h"
//common includes
#include <byteorder.h>
#include <contract.h>
#include <pointers.h>
//library includes
#include <binary/format_factories.h>
//std includes
#include <cstring>
//boost includes
#include <boost/array.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <formats/text/chiptune.h>

namespace Formats
{
namespace Chiptune
{
  namespace AYC
  {
    const std::size_t MAX_REGISTERS = 14;
  
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct BufferDescription
    {
      uint8_t SizeHi;
      uint16_t Offset;
      
      std::size_t GetAbsoluteOffset(uint_t idx) const
      {
        return fromLE(Offset) + idx * sizeof(*this) + 4;
      }
    } PACK_POST;
    
    PACK_PRE struct Header
    {
      uint16_t Duration;
      boost::array<BufferDescription, MAX_REGISTERS> Buffers;
      uint8_t Reserved[6];
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    BOOST_STATIC_ASSERT(sizeof(BufferDescription) == 3);
    BOOST_STATIC_ASSERT(sizeof(Header) == 50);
    
    const std::size_t MIN_SIZE = sizeof(Header) + 14;

    class StubBuilder : public Builder
    {
    public:
      virtual void SetFrames(std::size_t /*count*/) {}
      virtual void StartChannel(uint_t /*idx*/) {}
      virtual void AddValues(const Dump& /*values*/) {}
    };

    bool FastCheck(const Binary::Container& rawData)
    {
      if (rawData.Size() <= sizeof(Header))
      {
        return false;
      }
      const Header& header = *static_cast<const Header*>(rawData.Start());
      std::size_t minDataStart = sizeof(header);
      for (uint_t reg = 0; reg != header.Buffers.size(); ++reg)
      {
        const BufferDescription& buf = header.Buffers[reg];
        if (buf.SizeHi != 1 && buf.SizeHi != 4)
        {
          return false;
        }
        const std::size_t dataOffset = buf.GetAbsoluteOffset(reg);
        if (dataOffset < minDataStart)
        {
          return false;
        }
        minDataStart = dataOffset;
      }
      return true;
    }

    const std::string FORMAT(
      "?00-75"             //10 min approx
      "01|04 2e00"         //assume first chunk is right after header
      "(01|04 ?00-80){13}" //no more than 32k
      "ff{6}"
    );

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::CreateFormat(FORMAT, MIN_SIZE))
      {
      }

      virtual String GetDescription() const
      {
        return Text::AYC_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual bool Check(const Binary::Container& rawData) const
      {
        return FastCheck(rawData);
      }

      virtual Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const
      {
        Builder& stub = GetStubBuilder();
        return Parse(rawData, stub);
      }
    private:
      const Binary::Format::Ptr Format;
    };
    
    class Stream
    {
    public:
      Stream(uint_t sizeHi, const uint8_t* begin, const uint8_t* end)
        : SizeHi(sizeHi)
        , Cursor(begin)
        , Limit(end)
      {
      }
      
      uint_t GetBufferSize() const
      {
        return SizeHi << 8;
      }
      
      uint8_t ReadByte()
      {
        Require(Cursor < Limit);
        return *Cursor++;
      }
      
      uint_t ReadCounter()
      {
        const uint8_t val = -ReadByte();
        return val ? val : 256;
      }
      
      std::size_t ReadBackRef()
      {
        const std::size_t lo = ReadByte();
        return SizeHi == 1
          ? lo
          : lo | 256 * ReadByte();
      }
      
      const uint8_t* GetCursor() const
      {
        return Cursor;
      }
    private:
      const uint_t SizeHi;
      const uint8_t* Cursor;
      const uint8_t* const Limit;
    };
    
    void ParseBuffer(uint_t count, Stream& source, Builder& target)
    {
      const std::size_t bufSize = source.GetBufferSize();
      Dump buf(bufSize);
      std::size_t cursor = 0;
      uint_t flag = 0x40;
      //dX_flag
      while (count)
      {
        //dX_next
        flag <<= 1;
        if ((flag & 0xff) == 0)
        {
          flag = source.ReadByte();
          flag = (flag << 1) | 1;
        }
        if ((flag & 0x100) != 0)
        {
          flag &= 0xff;
          uint_t counter = source.ReadCounter();
          std::size_t srcPtr = source.ReadBackRef();
          Require(count >= counter);
          count -= counter;
          while (counter--)
          {
            buf[cursor++] = buf[srcPtr++];
            if (cursor >= bufSize)
            {
              target.AddValues(buf);
              cursor -= bufSize;
            }
            if (srcPtr >= bufSize)
            {
              srcPtr -= bufSize;
            }
          }
        }
        else
        {
          //dX_chr
          --count;
          buf[cursor++] = source.ReadByte();
          if (cursor >= bufSize)
          {
            target.AddValues(buf);
            cursor -= bufSize;
          }
        }
      }
      if (cursor)
      {
        buf.resize(cursor);
        target.AddValues(buf);
      }
    }

    Formats::Chiptune::Container::Ptr Parse(const Binary::Container& rawData, Builder& target)
    {
      if (!FastCheck(rawData))
      {
        return Formats::Chiptune::Container::Ptr();
      }

      try
      {
        const std::size_t size = rawData.Size();
        const uint8_t* const begin = static_cast<const uint8_t*>(rawData.Start());
        const uint8_t* const end = begin + size;
        const Header& header = *safe_ptr_cast<const Header*>(begin);
        const uint_t frames = fromLE(header.Duration);
        target.SetFrames(frames);
        std::size_t usedBegin = size;
        std::size_t usedEnd = 0;
        for (uint_t reg = 0; reg != header.Buffers.size(); ++reg)
        {
          target.StartChannel(reg);
          const BufferDescription& buf = header.Buffers[reg];
          const std::size_t offset = buf.GetAbsoluteOffset(reg);
          Require(offset < size);
          Stream stream(buf.SizeHi, begin + offset, end);
          ParseBuffer(frames, stream, target);
          usedBegin = std::min(usedBegin, offset);
          usedEnd = std::max<std::size_t>(usedEnd, stream.GetCursor() - begin);
        }
        const Binary::Container::Ptr subData = rawData.GetSubcontainer(0, usedEnd);
        return CreateCalculatingCrcContainer(subData, usedBegin, usedEnd - usedBegin);
      }
      catch (const std::exception&)
      {
        return Formats::Chiptune::Container::Ptr();
      }
    }

    Builder& GetStubBuilder()
    {
      static StubBuilder stub;
      return stub;
    }
  }//namespace AYC

  Decoder::Ptr CreateAYCDecoder()
  {
    return boost::make_shared<AYC::Decoder>();
  }
}//namespace Chiptune
}//namespace Formats
