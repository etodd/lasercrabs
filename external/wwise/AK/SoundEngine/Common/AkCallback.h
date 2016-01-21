//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkCallback.h

/// \file 
/// Declaration of callback prototypes


#ifndef _AK_CALLBACK_H_
#define _AK_CALLBACK_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkMidiTypes.h>

namespace AK
{
	class IAkMixerInputContext;
	class IAkMixerPluginContext;
}

/// Type of callback. Used as a bitfield in methods AK::SoundEngine::PostEvent() and AK::SoundEngine::DynamicSequence::Open().
enum AkCallbackType
{
	AK_EndOfEvent					= 0x0001,	///< Callback triggered when reaching the end of an event.
	AK_EndOfDynamicSequenceItem		= 0x0002,	///< Callback triggered when reaching the end of a dynamic sequence item.
	AK_Marker						= 0x0004,	///< Callback triggered when encountering a marker during playback.
	AK_Duration						= 0x0008,	///< Callback triggered when the duration of the sound is known by the sound engine.

	AK_SpeakerVolumeMatrix			= 0x0010,   ///< Callback triggered at each frame, letting the client modify the speaker volume matrix

	AK_Starvation					= 0x0020,	///< Callback triggered when playback skips a frame due to stream starvation.

	AK_MusicPlaylistSelect			= 0x0040,	///< Callback triggered when music playlist container must select the next item to play.
	AK_MusicPlayStarted				= 0x0080,	///< Callback triggered when a "Play" or "Seek" command has been executed ("Seek" commands are issued from AK::SoundEngine::SeekOnEvent()). Applies to objects of the Interactive-Music Hierarchy only.

	AK_MusicSyncBeat				= 0x0100,	///< Enable notifications on Music Beat.
	AK_MusicSyncBar					= 0x0200,	///< Enable notifications on Music Bar.
	AK_MusicSyncEntry				= 0x0400,	///< Enable notifications on Music Entry Cue.
	AK_MusicSyncExit				= 0x0800,	///< Enable notifications on Music Exit Cue.
	AK_MusicSyncGrid				= 0x1000,	///< Enable notifications on Music Grid.
	AK_MusicSyncUserCue				= 0x2000,	///< Enable notifications on Music Custom Cue.
	AK_MusicSyncPoint				= 0x4000,	///< Enable notifications on Music switch transition synchronisation point.
	AK_MusicSyncAll					= 0x7f00,	///< Use this flag if you want to receive all notifications concerning AK_MusicSync registration.

	AK_MidiEvent					= 0x10000,	///< Enable notifications for MIDI events.

	AK_CallbackBits					= 0xfffff,	///< Bitmask for all callback types.

	// Not callback types, but need to be part of same bitfield for AK::SoundEngine::PostEvent().
	AK_EnableGetSourcePlayPosition	= 0x100000,	///< Enable play position information for use by AK::SoundEngine::GetSourcePlayPosition().
	AK_EnableGetMusicPlayPosition	= 0x200000,	///< Enable play position information of music objects, queried via AK::MusicEngine::GetPlayingSegmentInfo().
	AK_EnableGetSourceStreamBuffering = 0x400000	///< Enable stream buffering information for use by AK::SoundEngine::GetSourceStreamBuffering(). 
};

/// Callback information structure used as base for all notifications handled by AkCallbackFunc.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
struct AkCallbackInfo
{
	void *			pCookie;		///< User data, passed to PostEvent()
	AkGameObjectID	gameObjID;		///< Game object ID
};

/// Callback information structure corresponding to AK_EndOfEvent, AK_MusicPlayStarted and AK_Starvation.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
struct AkEventCallbackInfo : public AkCallbackInfo
{
	AkPlayingID		playingID;		///< Playing ID of Event, returned by PostEvent()
	AkUniqueID		eventID;		///< Unique ID of Event, passed to PostEvent()
};

/// Callback information structure corresponding to AK_MidiEvent
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
struct AkMidiEventCallbackInfo : public AkEventCallbackInfo
{
	AkMidiEvent		midiEvent;		///< MIDI event triggered by event.
};


/// Callback information structure corresponding to AK_Marker.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
/// - \ref soundengine_markers
struct AkMarkerCallbackInfo : public AkEventCallbackInfo
{
	AkUInt32	uIdentifier;		///< Cue point identifier
	AkUInt32	uPosition;			///< Position in the cue point (unit: sample frames)
	const char*	strLabel;			///< Label of the marker, read from the file
};

/// Callback information structure corresponding to AK_Duration.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
struct AkDurationCallbackInfo : public AkEventCallbackInfo
{
	AkReal32	fDuration;				///< Duration of the sound (unit: milliseconds )
	AkReal32	fEstimatedDuration;		///< Estimated duration of the sound depending on source settings such as pitch. (unit: milliseconds )
	AkUniqueID	audioNodeID;			///< Audio Node ID of playing item
	bool		bStreaming;				///< True if source is streaming, false otherwise.
};

/// Callback information structure corresponding to AK_EndOfDynamicSequenceItem.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - AK::SoundEngine::DynamicSequence::Open()
/// - \ref soundengine_events
struct AkDynamicSequenceItemCallbackInfo : public AkCallbackInfo
{
	AkPlayingID playingID;			///< Playing ID of Dynamic Sequence, returned by AK::SoundEngine:DynamicSequence::Open()
	AkUniqueID	audioNodeID;		///< Audio Node ID of finished item
	void*		pCustomInfo;		///< Custom info passed to the DynamicSequence::Open function
};

/// Callback information structure corresponding to AK_SpeakerVolumeMatrix, and passed to callbacks registered in RegisterBusVolumeCallback()
/// or PostEvent() with AK_SpeakerVolumeMatrix. These callbacks are called at every audio frame for every connection from an input (voice
/// or bus) to an output bus (standard or auxiliary), at the point when an input signal is about to be mixed into a mixing bus, but just before 
/// having been scaled in accordance to volumes authored in Wwise. The volumes are passed via this structure as pointers because they can be modified
/// in the callbacks. They are factored into two linear values ([0..1]): a common base value (pfBaseVolume), that is channel-agnostic and represents 
/// the collapsed gain of all volume changes in Wwise (sliders, actions, RTPC, attenuations, ...), and a matrix of gains per input/output channel, 
/// which depends on spatialization. Use the methods of AK::SpeakerVolumes::Matrix, defined in AkCommonDefs.h, to perform operations on them. 
/// Access each input channel of the volumes matrix with AK::SpeakerVolumes::Matrix::GetChannel(), passing it the input and output channel configuration.
/// Then, you may access each element of the output vector using the standard bracket [] operator. See AK::SpeakerVolumes for more details.
/// It is crucial that the processing done in the callback be lightweight and non-blocking.
/// \sa 
/// - \ref goingfurther_speakermatrixcallback
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_events
/// - AK::SoundEngine::RegisterBusVolumeCallback()
struct AkSpeakerVolumeMatrixCallbackInfo : public AkEventCallbackInfo
{
	AK::SpeakerVolumes::MatrixPtr pVolumes;		///< Pointer to volume matrix describing the contribution of each source channel to destination channels. Use methods of AK::SpeakerVolumes::Matrix to interpret them. 
	AkChannelConfig inputConfig;				///< Channel configuration of the voice/bus.
	AkChannelConfig outputConfig;				///< Channel configuration of the output bus.
	AkReal32 * pfBaseVolume;					///< Base volume, common to all channels.
	AkReal32 * pfEmitterListenerVolume;			///< Emitter-listener pair-specific gain. When there are multiple emitter-listener pairs, this volume equals 1, and pair gains are applied directly on the channel volume matrix pVolumes.
	AK::IAkMixerInputContext * pContext;		///< Context of the current voice/bus about to be mixed into the output bus with specified base volume and volume matrix.
	AK::IAkMixerPluginContext * pMixerContext;	///< Output mixing bus context. Use it to access a few useful panning and mixing services, as well as the ID of the output bus. NULL if pContext is the master audio bus.
};

/// Callback information structure corresponding to Ak_MusicPlaylistSelect.
/// Called when a music playlist container must select its next item to play.
/// The members uPlaylistSelection and uPlaylistItemDone are set by the sound
/// engine before the callback function call.  They are set to the next item
/// selected by the sound engine.  They are to be modified by the callback
/// function if the selection is to be changed.
/// \sa 
/// - \ref soundengine_events
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_music_callbacks
struct AkMusicPlaylistCallbackInfo : public AkEventCallbackInfo
{
	AkUniqueID playlistID;			///< ID of playlist node
	AkUInt32 uNumPlaylistItems;		///< Number of items in playlist node (may be segments or other playlists)
	AkUInt32 uPlaylistSelection;	///< Selection: set by sound engine, modified by callback function (if not in range 0 <= uPlaylistSelection < uNumPlaylistItems then ignored).
	AkUInt32 uPlaylistItemDone;		///< Playlist node done: set by sound engine, modified by callback function (if set to anything but 0 then the current playlist item is done, and uPlaylistSelection is ignored)
};

/// Callback information structure corresponding to Ak_MusicSync.
/// If you need the Tempo, you can compute it using the fBeatDuration
/// Tempo (beats per minute) = 60/fBeatDuration
/// \sa 
/// - \ref soundengine_events
/// - AK::SoundEngine::PostEvent()
/// - \ref soundengine_music_callbacks
struct AkMusicSyncCallbackInfo : public AkCallbackInfo
{
	AkPlayingID playingID;			///< Playing ID of Event, returned by PostEvent()
	AkCallbackType musicSyncType;	///< Would be either AK_MusicSyncEntry, AK_MusicSyncBeat, AK_MusicSyncBar, AK_MusicSyncExit, AK_MusicSyncGrid, AK_MusicSyncPoint or AK_MusicSyncUserCue.
	AkReal32 fBeatDuration;			///< Beat Duration in seconds.
	AkReal32 fBarDuration;			///< Bar Duration in seconds.
	AkReal32 fGridDuration;			///< Grid duration in seconds.
	AkReal32 fGridOffset;			///< Grid offset in seconds.
	char *	 pszUserCueName;		///< Cue name (UTF-8 string). Set for notifications AK_MusicSyncUserCue. NULL if cue has no name.
};

/// Function called on completion of an event, or when a marker is reached.
/// \param in_eType Type of callback.
/// \param in_pCallbackInfo Pointer to structure containing callback information. This pointer is invalidated as soon as the callback function returns.
/// \remarks An event is considered completed once all of its actions have been executed and all the playbacks in this events are terminated.
/// \remarks This callback is executed from the audio processing thread. The processing time in the callback function should be minimal. Having too much processing time could result in slowing down the audio processing.
/// \remarks Before waiting on an AK_EndOfEvent, make sure that the event is going to end. 
/// Some events can be continuously playing or infinitely looping, and the callback will not occur unless a specific stop event is sent to terminate the event.
/// \sa 
/// - AK::SoundEngine::PostEvent()
/// - AK::SoundEngine::DynamicSequence::Open()
/// - \ref soundengine_events
/// - \ref soundengine_markers
/// - \ref soundengine_music_callbacks
AK_CALLBACK( void, AkCallbackFunc )( 
	AkCallbackType in_eType,							///< Callback type.
	AkCallbackInfo* in_pCallbackInfo					///< Structure containing desired information. You can cast it to the proper sub-type, depending on the callback type.
	);

/// Function called on at every audio frame for the specified registered busses.
/// \sa 
/// - AkSpeakerVolumeMatrixCallbackInfo
/// - AK::SoundEngine::RegisterBusVolumeCallback()
/// - \ref goingfurther_speakermatrixcallback
AK_CALLBACK( void, AkBusCallbackFunc )( 
	AkSpeakerVolumeMatrixCallbackInfo* in_pCallbackInfo	///< Structure containing desired bus information. 
	);

/// Function called on at every audio frame for the specified registered busses, just after metering has been computed.
/// \sa 
/// - AK::SoundEngine::RegisterBusMeteringCallback()
/// - AK::IAkMetering
/// - \ref goingfurther_speakermatrixcallback
AK_CALLBACK( void, AkBusMeteringCallbackFunc )( 
	AK::IAkMetering * in_pMetering,						///< AK::IAkMetering interface for retrieving metering information.
	AkChannelConfig	in_channelConfig,					///< Channel configuration of the bus.
	AkMeteringFlags in_eMeteringFlags					///< Metering flags that were asked for in RegisterBusMeteringCallback(). You may only access corresponding meter values from in_pMeteringInfo. Others will fail.
	);

/// Callback prototype used with asynchronous bank load/unload requests.
/// This function is called when the bank request has been processed 
/// and indicates if it was successfully executed or if an error occurred.
/// \param in_bankID Identifier of the bank that was explicitly loaded/unloaded. 
/// In the case of PrepareEvent() or PrepareGameSyncs(), this value contains 
/// the AkUniqueID of the event/game sync that was prepared/unprepared, if the array contained only
/// one element. Otherwise, in_bankID equals AK_INVALID_UNIQUE_ID.
/// \param in_pInMemoryBankPtr Value returned when the unloaded bank was loaded using an in memory location
/// \param in_eLoadResult Result of the requested action.
///	- AK_Success: Load or unload successful.
/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() or PrepareGameSyncs() does not exist.
/// - AK_InsufficientMemory: Insufficient memory to store bank data.
/// - AK_BankReadError: I/O error.
/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
/// you used to generate the SoundBanks matches that of the SDK you are currently using.
/// - AK_InvalidFile: File specified could not be opened.
/// - AK_InvalidParameter: Invalid parameter.
/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
/// \param in_memPoolId ID of the memory pool in which the bank was explicitly loaded/unloaded. 
/// AK_DEFAULT_POOL_ID is returned whenever this callback is issued from an implicit bank load (PrepareEvent(), PrepareGameSyncs()), 
/// the bank memory was managed internally, an error occured, or in the case of.
/// \param in_pCookie Optional cookie that was passed to the bank request.
/// \remarks This callback is executed from the bank thread. The processing time in the callback function should be minimal. Having too much processing time could slow down the bank loading process.
/// \sa 
/// - AK::SoundEngine::LoadBank()
/// - AK::SoundEngine::UnloadBank()
/// - AK::SoundEngine::PrepareEvent()
/// - AK::SoundEngine::PrepareGameSyncs()
/// - \ref soundengine_banks
AK_CALLBACK( void, AkBankCallbackFunc )(
	AkUInt32		in_bankID,
	const void *	in_pInMemoryBankPtr,
	AKRESULT		in_eLoadResult,
	AkMemPoolId		in_memPoolId,
	void *			in_pCookie
	);

/// Callback prototype used for global callback registration.
/// \param in_bLastCall true when called for the last time, upon sound engine termination.
/// \remarks This callback is executed from the main audio thread. The processing time in the callback function should be minimal. Having too much processing time could cause voice starvation.
/// - AK::SoundEngine::RegisterGlobalCallback()
/// - AK::SoundEngine::UnregisterGlobalCallback()
AK_CALLBACK( void, AkGlobalCallbackFunc )(
	bool in_bLastCall
	);

#endif // _AK_CALLBACK_H_

