/*
Abstract:
  AYM-based modules support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __CORE_PLUGINS_PLAYERS_AY_BASE_H_DEFINED__
#define __CORE_PLUGINS_PLAYERS_AY_BASE_H_DEFINED__

//local includes
#include "aym_parameters_helper.h"
#include <core/plugins/players/streaming.h>
#include <core/plugins/players/tracking.h>
//common includes
#include <error.h>
//library includes
#include <devices/aym.h>
#include <sound/render_params.h>

namespace ZXTune
{
  namespace Module
  {
    typedef PatternCursorSet<Devices::AYM::CHANNELS> AYMPatternCursors;

    class AYMDataRenderer
    {
    public:
      typedef boost::shared_ptr<AYMDataRenderer> Ptr;

      virtual ~AYMDataRenderer() {}

      virtual void SynthesizeData(const TrackState& state, const AYM::TrackBuilder& track) = 0;
      virtual void Reset() = 0;
    };

    Devices::AYM::Receiver::Ptr CreateAYMReceiver(AYM::TrackParameters::Ptr params, Sound::MultichannelReceiver::Ptr target);

    Renderer::Ptr CreateAYMRenderer(AYM::ParametersHelper::Ptr params, StateIterator::Ptr iterator, AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device);

    Renderer::Ptr CreateAYMStreamRenderer(AYM::TrackParameters::Ptr params, Information::Ptr info, AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device);

    Renderer::Ptr CreateAYMTrackRenderer(AYM::TrackParameters::Ptr params, Information::Ptr info, TrackModuleData::Ptr data,
      AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device);
  }
}

#endif //__CORE_PLUGINS_PLAYERS_AY_BASE_H_DEFINED__
