//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKMONITORERROR_H
#define _AKMONITORERROR_H

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
    // Error monitoring.

	namespace Monitor
	{
		///  ErrorLevel
		enum ErrorLevel
		{
			ErrorLevel_Message	= (1<<0), // used as bitfield
			ErrorLevel_Error	= (1<<1),
			
			ErrorLevel_All = ErrorLevel_Message | ErrorLevel_Error
		};
		/// ErrorCode
		enum ErrorCode
		{
			ErrorCode_NoError = 0, // 0-based index into AK::Monitor::s_aszErrorCodes table 
			ErrorCode_FileNotFound, 
			ErrorCode_CannotOpenFile,
			ErrorCode_CannotStartStreamNoMemory,
			ErrorCode_IODevice,
			ErrorCode_IncompatibleIOSettings,

			ErrorCode_PluginUnsupportedChannelConfiguration,
			ErrorCode_PluginMediaUnavailable,
			ErrorCode_PluginInitialisationFailed,
			ErrorCode_PluginProcessingFailed,
			ErrorCode_PluginExecutionInvalid,
			ErrorCode_PluginAllocationFailed,

			ErrorCode_VorbisRequireSeekTable,
			ErrorCode_VorbisRequireSeekTableVirtual,

			ErrorCode_VorbisDecodeError,
			ErrorCode_AACDecodeError,
			
			ErrorCode_xWMACreateDecoderFailed,

			ErrorCode_ATRAC9CreateDecoderFailed,
			ErrorCode_ATRAC9CreateDecoderFailedChShortage,
			ErrorCode_ATRAC9DecodeFailed,
			ErrorCode_ATRAC9ClearContextFailed,
			ErrorCode_ATRAC9LoopSectionTooSmall,

			ErrorCode_InvalidAudioFileHeader,
			ErrorCode_AudioFileHeaderTooLarge,
			ErrorCode_FileTooSmall,

			ErrorCode_TransitionNotAccurateChannel,
			ErrorCode_TransitionNotAccurateStarvation,
			ErrorCode_NothingToPlay, 
			ErrorCode_PlayFailed,

			ErrorCode_StingerCouldNotBeScheduled,
			ErrorCode_TooLongSegmentLookAhead,
			ErrorCode_CannotScheduleMusicSwitch,
			ErrorCode_TooManySimultaneousMusicSegments,
			ErrorCode_PlaylistStoppedForEditing,
			ErrorCode_MusicClipsRescheduledAfterTrackEdit,

			ErrorCode_CannotPlaySource_Create,
			ErrorCode_CannotPlaySource_VirtualOff,
			ErrorCode_CannotPlaySource_TimeSkip,
			ErrorCode_CannotPlaySource_InconsistentState,
			ErrorCode_MediaNotLoaded,
			ErrorCode_VoiceStarving,
			ErrorCode_StreamingSourceStarving,
			ErrorCode_XMADecoderSourceStarving,
			ErrorCode_XMADecodingError,

			ErrorCode_PluginNotRegistered,
			ErrorCode_CodecNotRegistered,

			ErrorCode_EventIDNotFound,

			ErrorCode_InvalidGroupID,
			ErrorCode_SelectedChildNotAvailable,
			ErrorCode_SelectedNodeNotAvailable,
			ErrorCode_SelectedMediaNotAvailable,
			ErrorCode_NoValidSwitch,

			ErrorCode_SelectedNodeNotAvailablePlay,

			ErrorCode_FeedbackVoiceStarving,

			ErrorCode_BankLoadFailed,
			ErrorCode_BankUnloadFailed,
			ErrorCode_ErrorWhileLoadingBank,
			ErrorCode_InsufficientSpaceToLoadBank,
			
			ErrorCode_LowerEngineCommandListFull,

			ErrorCode_SeekNoMarker,
			ErrorCode_CannotSeekContinuous,
			ErrorCode_SeekAfterEof,

			ErrorCode_UnknownGameObjectEvent,
			ErrorCode_UnknownGameObject,

			ErrorCode_ExternalSourceNotResolved,
			ErrorCode_FileFormatMismatch,

			ErrorCode_CommandQueueFull,
			ErrorCode_CommandTooLarge,

			ErrorCode_ExecuteActionOnEvent,
			ErrorCode_StopAll,
			ErrorCode_StopPlayingID,

			ErrorCode_XMACreateDecoderLimitReached,
			ErrorCode_XMAStreamBufferTooSmall,

			ErrorCode_ModulatorScopeError_Inst,
			ErrorCode_ModulatorScopeError_Obj,

			Num_ErrorCodes // THIS STAYS AT END OF ENUM
		};

		/// Function prototype of local output function pointer.
		AK_CALLBACK( void, LocalOutputFunc )(
			ErrorCode in_eErrorCode,	///< Error code number value
			const AkOSChar* in_pszError,	///< Message or error string to be displayed
			ErrorLevel in_eErrorLevel,	///< Specifies whether it should be displayed as a message or an error
			AkPlayingID in_playingID,   ///< Related Playing ID if applicable, AK_INVALID_PLAYING_ID otherwise
			AkGameObjectID in_gameObjID ///< Related Game Object ID if applicable, AK_INVALID_GAME_OBJECT otherwise
			);

		extern const AkOSChar* s_aszErrorCodes[ Num_ErrorCodes ];

		/// Post a monitoring message or error code. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, PostCode )( 
			ErrorCode in_eError,		///< Message or error code to be displayed
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);
#ifdef AK_SUPPORT_WCHAR
		/// Post a unicode monitoring message or error string. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, PostString )( 
			const wchar_t* in_pszError,	///< Message or error string to be displayed
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);
#endif // #ifdef AK_SUPPORT_WCHAR
		/// Post a monitoring message or error string. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, PostString )( 
			const char* in_pszError,	///< Message or error string to be displayed
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);

		/// Enable/Disable local output of monitoring messages or errors. Pass 0 to disable,
		/// or any combination of ErrorLevel_Message and ErrorLevel_Error to enable. 
		/// \return AK_Success.
		///			In optimized/release configuration, this function returns AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, SetLocalOutput )(
			AkUInt32 in_uErrorLevel	= ErrorLevel_All, ///< ErrorLevel(s) to enable in output. Default parameters enable all.
			LocalOutputFunc in_pMonitorFunc = 0 	  ///< Handler for local output. If NULL, the standard platform debug output method is used.
			);

		/// Get the time stamp shown in the capture log along with monitoring messages.
		/// \return AK_Success.
		///			In optimized/release configuration, this function returns 0.
		AK_EXTERNAPIFUNC( AkTimeMs, GetTimeStamp )();
	}
}

// Macros.
#ifndef AK_OPTIMIZED
    #define AK_MONITOR_ERROR( in_eErrorCode )\
	AK::Monitor::PostCode( in_eErrorCode, AK::Monitor::ErrorLevel_Error )
#else
    #define AK_MONITOR_ERROR( in_eErrorCode )
#endif

#ifdef AK_MONITOR_IMPLEMENT_ERRORCODES
namespace AK
{
	namespace Monitor
	{
		const AkOSChar* s_aszErrorCodes[ Num_ErrorCodes ] =
		{
			AKTEXT("No error"), // ErrorCode_NoError
			AKTEXT("File not found"), // ErrorCode_FileNotFound,
			AKTEXT("Cannot open file"), // ErrorCode_CannotOpenFile,
			AKTEXT("Not enough memory to start stream"), // ErrorCode_CannotStartStreamNoMemory,
			AKTEXT("IO device error"), // ErrorCode_IODevice,
			AKTEXT("IO settings incompatible with user requirements"), // ErrorCode_IncompatibleIOSettings

			AKTEXT("Plug-in unsupported channel configuration"), // ErrorCode_PluginUnsupportedChannelConfiguration,
			AKTEXT("Plug-in media unavailable"), // ErrorCode_PluginMediaUnavailable,
			AKTEXT("Plug-in initialization failure"), // ErrorCode_PluginInitialisationFailed,
			AKTEXT("Plug-in execution failure"), // ErrorCode_PluginProcessingFailed,
			AKTEXT("Invalid plug-in execution mode"), // ErrorCode_PluginExecutionInvalid
			AKTEXT("Could not allocate effect"), // ErrorCode_PluginAllocationFailed

			AKTEXT("Seek table required to seek in Vorbis sources. Please update conversion settings."), // ErrorCode_VorbisRequireSeekTable,
			AKTEXT("Seek table needed for Vorbis audio format with this virtual voice behavior. Please update conversion settings or virtual mode."), // ErrorCode_VorbisRequireSeekTableVirtual,

			AKTEXT("Vorbis decoder failure"), // ErrorCode_VorbisDecodeError,
			AKTEXT("AAC decoder failure"), // ErrorCode_AACDecodeError

			AKTEXT("Failed creating xWMA decoder"), // ErrorCode_xWMACreateDecoderFailed,

			AKTEXT("Failed creating ATRAC9 decoder"), // ErrorCode_ATRAC9CreateDecoderFailed
			AKTEXT("Failed creating ATRAC9 decoder: no more ATRAC9 decoding channels available"), // ErrorCode_ATRAC9CreateDecoderFailedChShortage
			AKTEXT("ATRAC9 decoding failed"), // ErrorCode_ATRAC9DecodeFailed
			AKTEXT("ATRAC9 context clear failed"), // ErrorCode_ATRAC9ClearContextFailed
			AKTEXT("ATRAC9 loop section is too small"), // ErrorCode_ATRAC9LoopSectionTooSmall

			AKTEXT("Invalid file header"), // ErrorCode_InvalidAudioFileHeader,
			AKTEXT("File header too large (due to markers or envelope)"), // ErrorCode_AudioFileHeaderTooLarge,
			AKTEXT("File or loop region is too small to be played properly"), // ErrorCode_FileTooSmall,

			AKTEXT("Transition not sample-accurate due to mixed channel configurations"), // ErrorCode_TransitionNotAccurateChannel,
			AKTEXT("Transition not sample-accurate due to source starvation"), // ErrorCode_TransitionNotAccurateStarvation,
			AKTEXT("Nothing to play"), // ErrorCode_NothingToPlay, 
			AKTEXT("Play Failed"), // ErrorCode_PlayFailed,	// Notification meaning the play asked was not done for an out of control reason
											// For example, if The Element has a missing source file.

			AKTEXT("Stinger could not be scheduled in this segment or was dropped"), // ErrorCode_StingerCouldNotBeScheduled,
			AKTEXT("Segment look-ahead is longer than previous segment in sequence"), // ErrorCode_TooLongSegmentLookAhead,
			AKTEXT("Cannot schedule music switch transition in upcoming segments: using Exit Cue"), // ErrorCode_CannotScheduleMusicSwitch,
			AKTEXT("Cannot schedule music segments: Stopping music"), // ErrorCode_TooManySimultaneousMusicSegments,
			AKTEXT("Music system is stopped because a music playlist is modified"), // ErrorCode_PlaylistStoppedForEditing
			AKTEXT("Rescheduling music clips because a track was modified"), // ErrorCode_MusicClipsRescheduledAfterTrackEdit

			AKTEXT("Failed creating source"), // ErrorCode_CannotPlaySource_Create,
			AKTEXT("Virtual source failed becoming physical"), // ErrorCode_CannotPlaySource_VirtualOff,
			AKTEXT("Error while computing virtual source elapsed time"), // ErrorCode_CannotPlaySource_TimeSkip,
			AKTEXT("Inconsistent source status"), // ErrorCode_CannotPlaySource_InconsistentState,
			AKTEXT("Media was not loaded for this source"),// ErrorCode_MediaNotLoaded,
			AKTEXT("Voice Starvation"), // ErrorCode_VoiceStarving,
			AKTEXT("Source starvation"), // ErrorCode_StreamingSourceStarving,
			AKTEXT("XMA decoder starvation"), // ErrorCode_XMADecoderSourceStarving,
			AKTEXT("XMA decoding error"), // ErrorCode_XMADecodingError

			AKTEXT("Plug-in not registered"), // ErrorCode_PluginNotRegistered,
			AKTEXT("Codec plug-in not registered"), // ErrorCode_CodecNotRegistered,

			AKTEXT("Event ID not found"), // ErrorCode_EventIDNotFound,

			AKTEXT("Invalid State Group ID"), // ErrorCode_InvalidGroupID,
			AKTEXT("Selected Child Not Available"), // ErrorCode_SelectedChildNotAvailable,
			AKTEXT("Selected Node Not Available"), // ErrorCode_SelectedNodeNotAvailable,
			AKTEXT("Selected Media Not Available"),// ErrorCode_SelectedMediaNotAvailable,
			AKTEXT("No Valid Switch"), // ErrorCode_NoValidSwitch,

			AKTEXT("Selected node not available. Make sure the structure associated to the event is loaded or that the event has been prepared"), // ErrorCode_SelectedNodeNotAvailablePlay,

			AKTEXT("Motion voice starvation"), // ErrorCode_FeedbackVoiceStarving,

			AKTEXT("Bank Load Failed"), // ErrorCode_BankLoadFailed,
			AKTEXT("Bank Unload Failed"), // ErrorCode_BankUnloadFailed,
			AKTEXT("Error while loading bank"), // ErrorCode_ErrorWhileLoadingBank,
			AKTEXT("Insufficient Space to Load Bank"), // ErrorCode_InsufficientSpaceToLoadBank,

			AKTEXT("Lower engine command list is full"), // ErrorCode_LowerEngineCommandListFull,

			AKTEXT("No marker in file; seeking to specified location"), // ErrorCode_SeekNoMarker
			AKTEXT("Cannot seek in sound that is within a continuous container with special transitions"), // ErrorCode_CannotSeekContinuous
			AKTEXT("Seeking after end of file. Playback will stop"), // ErrorCode_SeekAfterEof

			AKTEXT("Unknown game object ID. Make sure the game object is registered before using it and do not use it once it was unregistered."), // ErrorCode_UnknownGameObject,
			AKTEXT("Unknown game object ID. Make sure the game object is registered before using it and do not use it once it was unregistered."), // ErrorCode_UnknownGameObjectEvent

			AKTEXT("External source missing from PostEvent call"), // ErrorCode_ExternalSourceNotResolved
			AKTEXT("Source file is of different format than expected"), //ErrorCode_FileFormatMismatch
			AKTEXT("Audio command queue is full, blocking caller.  Reduce number of calls to sound engine or boost command queue memory."), // ErrorCode_CommandQueueFull
			AKTEXT("Audio command is too large to fit in the command queue.  Break the command in smaller pieces."), //ErrorCode_CommandTooLarge

			AKTEXT("ExecuteActionOnEvent API called"), // ErrorCode_ExecuteActionOnEvent
			AKTEXT("StopAll API called"), // ErrorCode_StopAll
			AKTEXT("StopPlayingID API called"), // ErrorCode_StopPlayingID

			AKTEXT("Failed creating XMA decoder: no more XMA voices available"), // ErrorCode_XMACreateDecoderLimitReached
			AKTEXT("Failed seeking in XMA source: stream buffer is smaller than XMA block size"), // ErrorCode_XMAStreamBufferTooSmall

			AKTEXT("Triggered a note-scoped or playing-instance-scoped modulator in a global context (such as a bus or bus effect).  Modulator will have global scope."), // ErrorCode_ModulatorScopeError_Inst
			AKTEXT("Triggered a game-object-scoped modulator in a global context (such as a bus or bus effect).  Modulator will have global scope.") // ErrorCode_ModulatorScopeError_Obj
		};
	}
}
#endif // AK_MONITOR_IMPLEMENT_ERRORCODES

#endif // _AKMONITORERROR_H
