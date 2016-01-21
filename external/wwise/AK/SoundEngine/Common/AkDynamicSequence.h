//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_SOUNDENGINE_AKDYNAMICSEQUENCE_H
#define _AK_SOUNDENGINE_AKDYNAMICSEQUENCE_H

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkArray.h>

class AkExternalSourceArray;

namespace AK
{
	namespace SoundEngine
	{
		/// Dynamic Sequence namespace
		/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
		namespace DynamicSequence
		{
			/// Playlist Item for Dynamic Sequence Playlist.
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::Playlist
			/// - AK::SoundEngine::PostEvent
			/// - \ref integrating_external_sources
			class PlaylistItem
			{
			public:
				PlaylistItem();
				PlaylistItem(const PlaylistItem& in_rCopy);
				~PlaylistItem();

				PlaylistItem& operator=(const PlaylistItem& in_rCopy);
				bool operator==(const PlaylistItem& in_rCopy)
				{
					AKASSERT(pExternalSrcs == NULL);
					return audioNodeID == in_rCopy.audioNodeID && 
						msDelay == in_rCopy.msDelay && 
						pCustomInfo == in_rCopy.pCustomInfo;
				};

				/// Sets the external sources used by this item.
				/// \sa 
				/// \ref integrating_external_sources
				AKRESULT SetExternalSources(AkUInt32 in_nExternalSrc, AkExternalSourceInfo* in_pExternalSrc);

				/// Get the external source array.  Internal use only.
				AkExternalSourceArray* GetExternalSources(){return pExternalSrcs;}

				AkUniqueID audioNodeID;	///< Unique ID of Audio Node
				AkTimeMs   msDelay;		///< Delay before playing this item, in milliseconds
				void *	   pCustomInfo;	///< Optional user data

			private:
				AkExternalSourceArray *pExternalSrcs;
			};

			/// List of items to play in a Dynamic Sequence.
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::LockPlaylist
			/// - AK::SoundEngine::DynamicSequence::UnlockPlaylist
			class Playlist
				: public AkArray<PlaylistItem, const PlaylistItem&, ArrayPoolDefault, 4>
			{
			public:
				/// Enqueue an Audio Node.
				/// \return AK_Success if successful, AK_Fail otherwise
				AkForceInline AKRESULT Enqueue( 
					AkUniqueID in_audioNodeID,		///< Unique ID of Audio Node
					AkTimeMs in_msDelay = 0,		///< Delay before playing this item, in milliseconds
					void * in_pCustomInfo = NULL,	///< Optional user data
					AkUInt32 in_cExternals = 0,					///< Optional count of external source structures
					AkExternalSourceInfo *in_pExternalSources = NULL///< Optional array of external source resolution information
					)
				{
					PlaylistItem * pItem = AddLast();
					if ( !pItem )
						return AK_Fail;

					pItem->audioNodeID = in_audioNodeID;
					pItem->msDelay = in_msDelay;
					pItem->pCustomInfo = in_pCustomInfo;
					return pItem->SetExternalSources(in_cExternals, in_pExternalSources);
				}
			};

			/// The DynamicSequenceType is specified when creating a new dynamic sequence.\n
			/// \n
			/// The default option is DynamicSequenceType_SampleAccurate. \n
			/// \n
			/// In sample accurate mode, when a dynamic sequence item finishes playing and there is another item\n
			/// pending in its playlist, the next sound will be stitched to the end of the ending sound. In this \n
			/// mode, if there are one or more pending items in the playlist while the dynamic sequence is playing,\n 
			/// or if something is added to the playlist during the playback, the dynamic sequence\n
			/// can remove the next item to be played from the playlist and prepare it for sample accurate playback before\n 
			/// the first sound is finished playing. This mechanism helps keep sounds sample accurate, but then\n
			/// you might not be able to remove that next item from the playlist if required.\n
			/// \n
			/// If your game requires the capability of removing the next to be played item from the\n
			/// playlist at any time, then you should use the DynamicSequenceType_NormalTransition option  instead.\n
			/// In this mode, you cannot ensure sample accuracy between sounds.\n
			/// \n
			/// Note that a Stop or a Break will always prevent the next to be played sound from actually being played.
			///
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::Open
			enum DynamicSequenceType
			{
				DynamicSequenceType_SampleAccurate,			///< Sample accurate mode
				DynamicSequenceType_NormalTransition		///< Normal transition mode, allows the entire playlist to be edited at all times.
			};

			/// Open a new Dynamic Sequence.
	        /// \return Playing ID of the dynamic sequence, or AK_INVALID_PLAYING_ID in failure case
			///
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::DynamicSequenceType
			AK_EXTERNAPIFUNC( AkPlayingID, Open )(
				AkGameObjectID		in_gameObjectID,			///< Associated game object ID
				AkUInt32			in_uFlags	   = 0,			///< Bitmask: see \ref AkCallbackType
				AkCallbackFunc		in_pfnCallback = NULL,		///< Callback function
				void* 				in_pCookie	   = NULL,		///< Callback cookie that will be sent to the callback function along with additional information;
				DynamicSequenceType in_eDynamicSequenceType = DynamicSequenceType_SampleAccurate ///< See : \ref AK::SoundEngine::DynamicSequence::DynamicSequenceType
				);
														
			/// Close specified Dynamic Sequence. The Dynamic Sequence will play until finished and then
			/// deallocate itself.
			AK_EXTERNAPIFUNC( AKRESULT, Close )(
				AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
				);

			/// Play specified Dynamic Sequence.
			AK_EXTERNAPIFUNC( AKRESULT, Play )( 
				AkPlayingID in_playingID,											///< AkPlayingID returned by DynamicSequence::Open
				AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
				AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
				);

			/// Pause specified Dynamic Sequence. 
			/// To restart the sequence, call Resume.  The item paused will resume its playback, followed by the rest of the sequence.
			AK_EXTERNAPIFUNC( AKRESULT, Pause )( 
				AkPlayingID in_playingID,											///< AkPlayingID returned by DynamicSequence::Open
				AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
				AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
				);

			/// Resume specified Dynamic Sequence.
			AK_EXTERNAPIFUNC( AKRESULT, Resume )(
				AkPlayingID in_playingID,											///< AkPlayingID returned by DynamicSequence::Open
				AkTimeMs in_uTransitionDuration = 0,									///< Fade duration
				AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
				);

			/// Stop specified Dynamic Sequence immediately.  
			/// To restart the sequence, call Play. The sequence will restart with the item that was in the 
			/// playlist after the item that was stopped.
			AK_EXTERNAPIFUNC( AKRESULT, Stop )(
				AkPlayingID in_playingID,											///< AkPlayingID returned by DynamicSequence::Open
				AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
				AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
				);

			/// Break specified Dynamic Sequence.  The sequence will stop after the current item.
			AK_EXTERNAPIFUNC( AKRESULT, Break )(
				AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
				);

			/// Lock the Playlist for editing. Needs a corresponding UnlockPlaylist call.
			/// \return Pointer to locked Playlist if successful, NULL otherwise
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::UnlockPlaylist
			AK_EXTERNAPIFUNC( Playlist *, LockPlaylist )(
				AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
				);

			/// Unlock the playlist.
			/// \sa
			/// - AK::SoundEngine::DynamicSequence::LockPlaylist
			AK_EXTERNAPIFUNC( AKRESULT, UnlockPlaylist )(
				AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
				);
		}
	}
}

#endif // _AK_SOUNDENGINE_AKDYNAMICSEQUENCE_H
