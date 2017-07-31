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

// AkMusicEngine.h

/// \file 
/// The main music engine interface.


#ifndef _AK_MUSICENGINE_H_
#define _AK_MUSICENGINE_H_

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCallback.h>

/// Platform-independent initialization settings of the music engine
/// \sa 
/// - AK::MusicEngine::Init()
/// - \ref soundengine_integration_init_advanced
struct AkMusicSettings
{
	AkReal32 fStreamingLookAheadRatio;	///< Multiplication factor for all streaming look-ahead heuristic values.
};

// Audiokinetic namespace
namespace AK
{
	/// Music engine namespace
	/// \warning The functions in this namespace are not thread-safe, unless stated otherwise.
	namespace MusicEngine
	{
        ///////////////////////////////////////////////////////////////////////
		/// @name Initialization
		//@{

		/// Initialize the music engine.
		/// \warning This function must be called after the base sound engine has been properly initialized. 
		/// There should be no AK API call between AK::SoundEngine::Init() and this call. Any call done in between is potentially unsafe.
		/// \return AK_Success if the Init was successful, AK_Fail otherwise.
		/// \sa
		/// - \ref workingwithsdks_initialization
        AK_EXTERNAPIFUNC( AKRESULT, Init )(
			AkMusicSettings *	in_pSettings	///< Initialization settings (can be NULL, to use the default values)
			);

		/// Get the music engine's default initialization settings values
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::MusicEngine::Init()
		AK_EXTERNAPIFUNC( void, GetDefaultInitSettings )(
            AkMusicSettings &	out_settings	///< Returned default platform-independent music engine settings
		    );

		/// Terminate the music engine.
		/// \warning This function must be called before calling Term() on the base sound engine.
		/// \sa
		/// - \ref workingwithsdks_termination
		AK_EXTERNAPIFUNC( void, Term )(
			);
		
		/// Query information on the active segment of a music object that is playing. Use the playing ID 
		/// that was returned from AK::SoundEngine::PostEvent(), provided that the event contained a play
		/// action that was targetting a music object. For any configuration of interactive music hierarchy, 
		/// there is only one segment that is active at a time. 
		/// To be able to query segment information, you must pass the AK_EnableGetMusicPlayPosition flag 
		/// to the AK::SoundEngine::PostEvent() method. This informs the sound engine that the source associated 
		/// with this event should be given special consideration because GetPlayingSegmentInfo() can be called 
		/// at any time for this AkPlayingID.
		/// Notes:
		/// - If the music object is a single segment, you will get negative values for AkSegmentInfo::iCurrentPosition
		///		during the pre-entry. This will never occur with other types of music objects because the 
		///		pre-entry of a segment always overlaps another active segment.
		///	- The active segment during the pre-entry of the first segment of a Playlist Container or a Music Switch 
		///		Container is "nothing", as well as during the post-exit of the last segment of a Playlist (and beyond).
		///	- When the active segment is "nothing", out_uSegmentInfo is filled with zeros.
		/// - If in_bExtrapolate is true (default), AkSegmentInfo::iCurrentPosition is corrected by the amount of time elapsed
		///		since the beginning of the audio frame. It is thus possible that it slightly overshoots the total segment length.
		/// \return AK_Success if there is a playing music structure associated with the specified playing ID.
		/// \sa
		/// - AK::SoundEngine::PostEvent
		/// - AkSegmentInfo
		AK_EXTERNAPIFUNC( AKRESULT, GetPlayingSegmentInfo )(
			AkPlayingID		in_PlayingID,			///< Playing ID returned by AK::SoundEngine::PostEvent().
			AkSegmentInfo &	out_segmentInfo,		///< Structure containing information about the active segment of the music structure that is playing.
			bool			in_bExtrapolate = true	///< Position is extrapolated based on time elapsed since last sound engine update.
			);

        //@}
    }
}

#endif // _AK_MUSICENGINE_H_

