/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Software source plug-in and effect plug-in interfaces.

#ifndef _IAK_MIXER_PLUGIN_H_
#define _IAK_MIXER_PLUGIN_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

namespace AK
{
	/// Software effect plug-in interface for panner/mixer effects (see \ref soundengine_plugins_effects).
	class IAkMixerEffectPlugin : public IAkPlugin
	{
	public:

		/// Software effect plug-in initialization. Prepares the effect for data processing, allocates memory and sets up the initial conditions. 
		/// \aknote Memory allocation should be done through appropriate macros (see \ref fx_memory_alloc). \endaknote
		virtual AKRESULT Init( 
			IAkPluginMemAlloc *			in_pAllocator,				///< Interface to memory allocator to be used by the effect.
			IAkMixerPluginContext *		in_pMixerPluginContext,		///< Interface to mixer plug-in's context.
			IAkPluginParam *			in_pParams,					///< Interface to plug-in parameters.
			AkAudioFormat &				in_rFormat					///< Audio data format of the requested output signal.
			) = 0;

		/// This function is called whenever a new input is connected to the underlying mixing bus.
		virtual void OnInputConnected( 
			IAkMixerInputContext * in_pInput			///< Input that is being connected.
			) = 0;
		
		/// This function is called whenever a new input is disconnected to the underlying mixing bus.
		/// \aknote OnInputDisconnected() may be called between calls to ConsumeInput() and OnMixDone().\endaknote
		virtual void OnInputDisconnected( 
			IAkMixerInputContext * in_pInput			///< Input that is being disconnected.
			) = 0;

		/// This function is called whenever an input (voice or bus) needs to be mixed.
		/// During an audio frame, ConsumeInput() will be called for each input that need to be mixed.
		/// \aknote io_pInputBuffer->eState will be set as AK_NoMoreData the last time the given input is processed by ConsumeInput(). Otherwise it is set to AK_DataReady.
		/// Mixers cannot make an input remain alive by changing their state.\endaknote
		/// \aknote ConsumeInput() will not be called for frames during which a voice is not audible.\endaknote
		/// \sa
		/// - OnMixDone
		/// - OnEffectsProcessed
		virtual void ConsumeInput( 
			IAkMixerInputContext *	in_pInputContext,	///< Context for this input. Carries non-audio data.
			AkRamp					in_baseVolume,		///< Base volume to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer). This gain is agnostic of emitter-listener pair-specific contributions (such as distance level attenuation).
			AkRamp					in_emitListVolume,	///< Emitter-listener pair-specific gain. When there are multiple emitter-listener pairs, this volume equals 1, and pair gains are applied directly on the channel volume matrix (accessible via IAkMixerInputContext::GetSpatializedVolumes()). For custom processing of emitter-listener pairs, one should query each pair volume using IAkMixerInputContext::Get3DPosition(), then AkEmitterListenerPair::GetGainForConnectionType().
			AkAudioBuffer *			io_pInputBuffer,	///< Input audio buffer data structure. Plugins should avoid processing data in-place.
			AkAudioBuffer *			io_pMixBuffer		///< Output audio buffer data structure. Stored until call to OnEffectsProcessed().
			) = 0;

		/// This function is called once every audio frame, when all inputs have been mixed in 
		/// with ConsumeInput(). It is the time when the plugin may perform final DSP/bookkeeping.
		/// After the bus buffer io_pMixBuffer has been pushed to the bus downstream (or the output device),
		/// it is cleared out for the next frame.
		/// \aknote io_pMixBuffer->eState is passed as AK_DataReady for the whole existence of the bus, until the last frame where it will be set to AK_NoMoreData.
		/// However, mixer plugins are capable of forcing the bus to remain alive for a longer time by changing io_pMixBuffer->eState back to AK_DataReady.
		/// You may do this in OnMixDone() or in OnEffectsProcessed(). The difference is that effects inserted on the bus will enter their "tail mode" if you
		/// wait until OnEffectsProcessed() to flip the state to AK_DataReady. This is usually undesirable, so handling this inside OnMixDone() is usually preferred.\endaknote
		/// \sa
		/// - ConsumeInput
		/// - OnEffectsProcessed
		virtual void OnMixDone( 
			AkAudioBuffer *			io_pMixBuffer		///< Output audio buffer data structure. Stored across calls to ConsumeInput().
			) = 0;

		/// This function is called once every audio frame, after all insert effects on the bus have been processed, 
		/// which occur after the post mix pass of OnMixDone().
		/// After the bus buffer io_pMixBuffer has been pushed to the bus downstream (or the output device),
		/// it is cleared out for the next frame.
		/// \aknote io_pMixBuffer->eState is passed as AK_DataReady for the whole existence of the bus, until the last frame where it will be set to AK_NoMoreData.
		/// However, mixer plugins are capable of forcing the bus to remain alive for a longer time by changing io_pMixBuffer->eState back to AK_DataReady.
		/// You may do this in OnMixDone(), in OnEffectsProcessed() or in OnFrameEnd(). The difference is that effects inserted on the bus will enter their "tail mode" if you
		/// wait until OnEffectsProcessed() or OnFrameEnd() to flip the state to AK_DataReady. This is usually undesirable, so handling this inside OnMixDone() is usually preferred.\endaknote
		/// \sa
		/// - OnMixDone
		/// - AK::IAkMetering
		/// - AK::IAkMixerPluginContext::EnableMetering()
		virtual void OnEffectsProcessed( 
			AkAudioBuffer *			io_pMixBuffer		///< Output audio buffer data structure.
			) = 0;

		/// This function is called once every audio frame, after all insert effects on the bus have been processed, and after metering has been processed (if applicable),
		/// which occur after OnEffectsProcessed().
		/// After the bus buffer io_pMixBuffer has been pushed to the bus downstream (or the output device), it is cleared out for the next frame.
		/// Mixer plugins may use this hook for processing the signal after it has been metered.
		/// \aknote io_pMixBuffer->eState is passed as AK_DataReady for the whole existence of the bus, until the last frame where it will be set to AK_NoMoreData.
		/// However, mixer plugins are capable of forcing the bus to remain alive for a longer time by changing io_pMixBuffer->eState back to AK_DataReady.
		/// You may do this in OnMixDone(), in OnEffectsProcessed() or in OnFrameEnd(). The difference is that effects inserted on the bus will enter their "tail mode" if you
		/// wait until OnEffectsProcessed() or OnFrameEnd() to flip the state to AK_DataReady. This is usually undesirable, so handling this inside OnMixDone() is usually preferred.\endaknote
		/// \aknote This function is called after metering gets computed on io_pMixBuffer. You may access the result in in_pMetering. Metering has to be enabled with AK::IAkMixerPluginContext::EnableMetering().
		/// It may also be enabled by the Wwise authoring tool when connected.\endaknote
		/// \sa
		/// - OnMixDone
		/// - AK::IAkMetering
		/// - AK::IAkMixerPluginContext::EnableMetering()
		virtual void OnFrameEnd(
			AkAudioBuffer *			io_pMixBuffer,		///< Output audio buffer data structure.
			IAkMetering *			in_pMetering		///< Interface for retrieving metering data computed on io_pMixBuffer. May be NULL if metering is not enabled.
			) = 0;
	};
}
#endif // _IAK_MIXER_PLUGIN_H_
