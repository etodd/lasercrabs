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

  Version: v2016.2.4  Build: 6097
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Main Sound Engine interface, PS4 specific.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

/// Platform specific initialization settings
/// \sa AK::SoundEngine::Init
/// \sa AK::SoundEngine::GetDefaultPlatformInitSettings
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
struct AkPlatformInitSettings
{
    // Threading model.
    AkThreadProperties  threadLEngine;			///< Lower engine threading properties
	AkThreadProperties  threadBankManager;		///< Bank manager threading properties (its default priority is AK_THREAD_PRIORITY_NORMAL)

    // Memory.
    AkUInt32            uLEngineDefaultPoolSize;///< Lower Engine default memory pool size
	AkReal32            fLEngineDefaultPoolRatioThreshold;	///< 0.0f to 1.0f value: The percentage of occupied memory where the sound engine should enter in Low memory mode. \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
	
	// (SCE_AJM_JOB_INITIALIZE_SIZE*MAX_INIT_SOUND_PER_FRAME) + (SCE_AJM_JOB_RUN_SPLIT_SIZE(4)*MAX_BANK_SRC + (SCE_AJM_JOB_RUN_SPLIT_SIZE(5)*MAX_FILE_SRC
	AkUInt32            uLEngineAcpBatchBufferSize; ///< Lower Engine default memory pool size
	bool				bHwCodecLowLatencyMode; ///< Use low latency mode for ATRAC9  (default is false).  If true, decoding jobs are submitted at the beginning of the Wwise update and it will be necessary to wait for the result.

	// Voices.
	AkUInt16            uNumRefillsInVoice;		///< Number of refill buffers in voice buffer. 2 == double-buffered, defaults to 4.	
	
	AkThreadProperties  threadMonitor;			///< Monitor threading properties (its default priority is AK_THREAD_PRIORITY_ABOVENORMAL). This parameter is not used in Release build.
};

///< Used with \ref AK::SoundEngine::AddSecondaryOutput to specify the type of secondary output.
enum AkAudioOutputType
{
	AkOutput_None = 0,		///< Used for uninitialized type, do not use.
	AkOutput_Dummy,			///< Dummy output, simply eats the audio stream and outputs nothing.
	AkOutput_MergeToMain,	///< This output will mix back its content to the main output, after the master mix.
	AkOutput_Main,			///< Main output.  This cannot be used with AddSecondaryOutput, but can be used to query information about the main output (GetSpeakerConfiguration for example).	
	AkOutput_Voice,			///< Use the PS4 voice channel.
	AkOutput_Personal,		///< Use the Personal channel (headset).
	AkOutput_PAD,			///< Use the controller speaker channel.
	AkOutput_BGM,			///< Output to background music port.	
	AkOutput_Aux,			///< Output to the auxiliary port of the PS4.
	AkOutput_NumBuiltInOutputs,		///< Do not use.
	AkOutput_Plugin			///< Specify if using Audio Device Plugin Sink.
};

enum AkAudioOutputFlags
{
	AkAudioOutputFlags_OptionNotRecordable = 1 << 0 ///< This is an optional flag to tell that this output should not be recorded by the internal DVR.  OR-it with the other flags.
};

namespace AK
{
	/// Returns the current PS4 output port handle being used by the Wwise SoundEngine for main output.
	/// This should be called only once the SoundEngine has been successfully initialized, otherwise
	/// the function will return an invalid value (-1).
	///
	/// \return the current PS4 main output port handle or -1.

	extern int GetPS4OutputHandle();
};
