/**
*
* @file      sound/receiver.h
* @brief     Defenition of sound receiver interface
* @version   $Id$
* @author    (C) Vitamin/CAIG/2001
*
**/

#pragma once
#ifndef __SOUND_RECEIVER_H_DEFINED__
#define __SOUND_RECEIVER_H_DEFINED__

//common includes
#include <data_streaming.h>
//library includes
#include <sound/sound_types.h>

namespace ZXTune
{
  namespace Sound
  {
    //! @brief Simple sound stream endpoint receiver
    typedef DataReceiver<OutputSample> Receiver;
    //! @brief Simple sound stream source
    typedef DataTransmitter<OutputSample> Transmitter;
    //! @brief Simle sound stream converter
    typedef DataTransceiver<OutputSample> Converter;
    //! @brief Multichannel stream receiver
    typedef DataReceiver<MultichannelSample> MultichannelReceiver;
  }
}

#endif //__SOUND_RECEIVER_H_DEFINED__
