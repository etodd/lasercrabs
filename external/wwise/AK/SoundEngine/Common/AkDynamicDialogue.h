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

#ifndef _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H
#define _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H

#include <AK/SoundEngine/Common/AkSoundEngine.h>

/// Callback prototype used with dialogue event resolution. This function is called
/// for every candidate in a ResolveDialogueEvent execution.
/// \return true to accept candidate, false to reject.
/// \sa 
/// - AK::SoundEngine::DynamicDialogue::ResolveDialogueEvent()
AK_CALLBACK( bool, AkCandidateCallbackFunc )(
	AkUniqueID in_idEvent,
	AkUniqueID in_idCandidate,
	void* in_cookie
	);

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
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID,	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
					AkCandidateCallbackFunc in_candidateCallbackFunc = NULL, ///< Optional callback for candidate validation based on custom criteria
					void* in_pCookie = NULL							///< Callback cookie (reserved to user, passed to the callback function)
				);

#ifdef AK_SUPPORT_WCHAR
			/// Resolve a dialogue event into an audio node ID based on the specified argument path.
	        /// \return Unique ID of audio node, or AK_INVALID_UNIQUE_ID if no audio node is defined for specified argument path
			AK_EXTERNAPIFUNC( AkUniqueID, ResolveDialogueEvent )(
					const wchar_t*		in_pszEventName,			///< Name of dialogue event
					const wchar_t**		in_aArgumentValueNames,		///< Argument path, as array of argument value names. L"" indicates a fallback argument value
					AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArgumentValueNames
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID,	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
					AkCandidateCallbackFunc in_candidateCallbackFunc = NULL, ///< Optional callback for candidate validation based on custom criteria
					void* in_pCookie = NULL							///< Callback cookie (reserved to user, passed to the callback function)
				);
#endif //AK_SUPPORT_WCHAR

			/// Resolve a dialogue event into an audio node ID based on the specified argument path.
	        /// \return Unique ID of audio node, or AK_INVALID_UNIQUE_ID if no audio node is defined for specified argument path
			AK_EXTERNAPIFUNC( AkUniqueID, ResolveDialogueEvent )(
					const char*			in_pszEventName,			///< Name of dialogue event
					const char**		in_aArgumentValueNames,		///< Argument path, as array of argument value names. "" indicates a fallback argument value
					AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArgumentValueNames
					AkPlayingID			in_idSequence = AK_INVALID_PLAYING_ID,	///< Optional sequence ID in which the token will be inserted (for profiling purposes)
					AkCandidateCallbackFunc in_candidateCallbackFunc = NULL, ///< Optional callback for candidate validation based on custom criteria
					void* in_pCookie = NULL							///< Callback cookie (reserved to user, passed to the callback function)
				);

			/// Get the value of a custom property of integer or boolean type.
			AK_EXTERNAPIFUNC(AKRESULT, GetDialogueEventCustomPropertyValue)(
				AkUniqueID in_eventID,			///< Unique ID of dialogue event
				AkUInt32 in_uPropID,			///< Property ID of your custom property found under the Custom Properties tab of the Wwise project settings.
				AkInt32& out_iValue				///< Property Value
				);

			/// Get the value of a custom property of real type.
			AK_EXTERNAPIFUNC(AKRESULT, GetDialogueEventCustomPropertyValue)(
				AkUniqueID in_eventID,			///< Unique ID of dialogue event
				AkUInt32 in_uPropID,			///< Property ID of your custom property found under the Custom Properties tab of the Wwise project settings.
				AkReal32& out_fValue			///< Property Value
				);
		}
	}
}

#endif // _AK_SOUNDENGINE_AKDYNAMICDIALOGUE_H
