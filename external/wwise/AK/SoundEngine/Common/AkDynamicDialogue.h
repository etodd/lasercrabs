//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H
#define _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace AK
{
	namespace SoundEngine
	{
		/// Dynamic Dialogue namespace
		/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
		namespace DynamicDialogue
		{
			/// Resolve a dialogue event into an audio node ID based on the specified argument path.
	        /// \return Unique ID of audio node, or AK_INVALID_UNIQUE_ID if no audio node is defined for specified argument path
			AK_EXTERNAPIFUNC( AkUniqueID, ResolveDialogueEvent )(
					AkUniqueID			in_eventID,					///< Unique ID of dialogue event
					AkArgumentValueID*	in_aArgumentValues,			///< Argument path, as array of argument value IDs. AK_FALLBACK_ARGUMENTVALUE_ID indicates a fallback argument value
					AkUInt32			in_uNumArguments,			///< Number of argument value IDs in in_aArgumentValues
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
				);

#ifdef AK_SUPPORT_WCHAR
			/// Resolve a dialogue event into an audio node ID based on the specified argument path.
	        /// \return Unique ID of audio node, or AK_INVALID_UNIQUE_ID if no audio node is defined for specified argument path
			AK_EXTERNAPIFUNC( AkUniqueID, ResolveDialogueEvent )(
					const wchar_t*		in_pszEventName,			///< Name of dialogue event
					const wchar_t**		in_aArgumentValueNames,		///< Argument path, as array of argument value names. L"" indicates a fallback argument value
					AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArgumentValueNames
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
				);
#endif //AK_SUPPORT_WCHAR

			/// Resolve a dialogue event into an audio node ID based on the specified argument path.
	        /// \return Unique ID of audio node, or AK_INVALID_UNIQUE_ID if no audio node is defined for specified argument path
			AK_EXTERNAPIFUNC( AkUniqueID, ResolveDialogueEvent )(
					const char*			in_pszEventName,			///< Name of dialogue event
					const char**		in_aArgumentValueNames,		///< Argument path, as array of argument value names. "" indicates a fallback argument value
					AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArgumentValueNames
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
				);
		}
	}
}

#endif // _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H
