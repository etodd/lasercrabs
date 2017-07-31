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
/// Wwise source control plug-in dialog interface, used to implement custom dialogs that have the Wwise look and feel.

#ifndef _AK_WWISE_ISOURCECONTROLDIALOGBASE_H
#define _AK_WWISE_ISOURCECONTROLDIALOGBASE_H

#include <AK/SoundEngine/Common/AkTypes.h>

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		/// Wwise dialog base interface. This must be implemented for each dialog that
		/// needs to be displayed with the Wwise look and feel.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - \ref source_control_dll_creation_dialog_implement
		class ISourceControlDialogBase
		{
		public:
			
			/// This function is called by Wwise to get the HINSTANCE used for loading resources.
			/// \return The HINSTANCE of the plug-in DLL resource.
			virtual HINSTANCE GetResourceHandle() const = 0;

			/// This function is called by Wwise to get the plug-in dialog's ID.
			virtual void GetDialog( 
				UINT & out_uiDialogID			///< The returned resource ID of the dialog
				) const = 0;

			/// Asks the dialog if it has Help associated with it.
			/// \return True if the '?' Help button should be shown on the dialog, False otherwise
			virtual bool HasHelp() const = 0;

			/// Called when the user clicks the '?' Help icon.
			/// \return True if the plug-in handled the Help request, False otherwise
			virtual bool Help( 
				HWND in_hWnd					///< The window handle of the dialog
				) const = 0;

			/// Window message handler for the dialog. This is very similar to a standard WIN32 window procedure.
			/// \return True if the message has been processed by the plug-in, False otherwise
			virtual bool WindowProc( 
				HWND in_hWnd,					///< The window handle of the dialog
				UINT in_message,				///< The incoming message. This is a standard Windows message ID (e.g. WM_PAINT)
				WPARAM in_wParam,				///< The WPARAM of the message (see MSDN)
				LPARAM in_lParam,				///< The LPARAM of the message (see MSDN)
				LRESULT & out_lResult			///< The returned value if the message has been processed. It is only considered if the method also returns True
				) = 0;
		};
	}
}

#endif // _AK_WWISE_ISOURCECONTROLDIALOGBASE_H