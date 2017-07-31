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
/// Wwise source control plug-in operation progress dialog interface, used to display progress during source control operations

#ifndef _AK_WWISE_ISOURCECONTROLOPERATIONPROGRESS_H
#define _AK_WWISE_ISOURCECONTROLOPERATIONPROGRESS_H

#include <AK/SoundEngine/Common/AkTypes.h>

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		/// Wwise progress dialog interface. This interface is given by AK::Wwise::ISourceControlUtilities.
		/// You can use this interface to display a simple progress dialog while performing operations.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - \ref source_control_dll_creation_progress
		class ISourceControlOperationProgress
		{
		public:

			/// Creates and displays the progress dialog.
			virtual void ShowProgress () = 0;

			/// Adds a new text line in the log message list. Note that escape characters (such as '\n') are not effective
			/// in this message list.
			virtual void AddLogMessage ( 
				LPCWSTR in_pszMessage		///< The text line to add to the list.
				) = 0;

			/// This function is used to know if the user clicked the 'Cancel' button.
			/// \return True if the user clicked 'Cancel', False otherwise
			virtual bool IsCanceled() const = 0;

			/// Manually cancels the operation. This result is the same as if the user pressed 'Cancel'. When using
			/// this function, AK:Wwise::ISourceControlOperationProgress::OperationCompleted() must be called to
			/// close the dialog.
			virtual void Cancel() = 0;

			/// Call this function when the operation is completed. 
			/// When in_bWaitForOK is true, the function will not return until the
			/// user clicks the 'OK' button. 
			/// The progress dialog will be destroyed.
			virtual void OperationCompleted( bool in_bWaitForOK = true ) = 0;
		};
	}
}

#endif // _AK_WWISE_ISOURCECONTROLOPERATIONPROGRESS_H