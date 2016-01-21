//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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